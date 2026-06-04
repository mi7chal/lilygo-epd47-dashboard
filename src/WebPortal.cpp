#include "WebPortal.h"
#include "Log.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <errno.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <fmt/format.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <esp_http_client.h>
#include <esp_http_server.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_crt_bundle.h>
#include <cJSON.h>

namespace {
constexpr uint16_t DNS_PORT = 53;
constexpr char AP_IP_STR[] = "192.168.4.1";
constexpr char HOSTNAME[] = "lilygo-dashboard";
constexpr char CAPTIVE_HTML[] = "<html><body style='font-family:sans-serif; text-align:center; margin-top:50px;'><h2>404 Not Found</h2><p>Site cannot be found.</p></body></html>";

std::string normalizeCoordinate(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char character) {
        return character == '\r' || character == '\n' || character == '\t';
    }), value.end());
    std::replace(value.begin(), value.end(), ',', '.');
    return value;
}

std::string trim_copy(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

std::string urlEncode(const std::string& value) {
    std::string encoded;
    encoded.reserve(value.size() * 3);
    const char* hex = "0123456789ABCDEF";
    for (unsigned char character : value) {
        if ((character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '-' || character == '_' || character == '.' || character == '~') {
            encoded.push_back(static_cast<char>(character));
        } else if (character == ' ') {
            encoded.push_back('+');
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[(character >> 4) & 0x0F]);
            encoded.push_back(hex[character & 0x0F]);
        }
    }
    return encoded;
}

std::string htmlEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char character : value) {
        switch (character) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped.push_back(character); break;
        }
    }
    return escaped;
}

bool readRequestBody(httpd_req_t* request, std::string& body) {
    const size_t totalSize = request->content_len;
    body.clear();
    body.reserve(totalSize + 1);

    size_t remaining = totalSize;
    char buffer[256];
    while (remaining > 0) {
        const int read = httpd_req_recv(request, buffer, std::min(remaining, sizeof(buffer)));
        if (read <= 0) {
            return false;
        }
        body.append(buffer, buffer + read);
        remaining -= static_cast<size_t>(read);
    }

    return true;
}

std::string getFormValue(const std::string& body, const std::string& key) {
    const std::string prefix = key + "=";
    const auto keyPos = body.find(prefix);
    if (keyPos == std::string::npos) {
        return {};
    }

    auto valueStart = keyPos + prefix.size();
    auto valueEnd = body.find('&', valueStart);
    if (valueEnd == std::string::npos) {
        valueEnd = body.size();
    }

    std::string raw = body.substr(valueStart, valueEnd - valueStart);
    std::string decoded;
    decoded.reserve(raw.size());
    for (size_t index = 0; index < raw.size(); ++index) {
        if (raw[index] == '+' ) {
            decoded.push_back(' ');
        } else if (raw[index] == '%' && index + 2 < raw.size()) {
            const auto hexValue = raw.substr(index + 1, 2);
            char* end = nullptr;
            const long parsed = std::strtol(hexValue.c_str(), &end, 16);
            if (end != hexValue.c_str()) {
                decoded.push_back(static_cast<char>(parsed));
                index += 2;
            }
        } else {
            decoded.push_back(raw[index]);
        }
    }

    return decoded;
}

std::string buildSettingsPage(const StorageManager& storage) {
    const std::string ssid = htmlEscape(storage.getSSID());
    const std::string addressLine = htmlEscape(storage.getAddressLine());
    const std::string city = htmlEscape(storage.getCity());
    const std::string postalCode = htmlEscape(storage.getPostalCode());
    const std::string country = htmlEscape(storage.getCountry());
    const std::string latitude = htmlEscape(storage.getLatitude());
    const std::string longitude = htmlEscape(storage.getLongitude());

    return fmt::format(
        R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>LilyGO Dashboard</title>
<style>body{{font-family:sans-serif;max-width:760px;margin:40px auto;padding:0 16px;line-height:1.4}}input{{width:100%;padding:10px;margin:6px 0 14px}}button{{padding:12px 16px}}</style></head>
<body><h1>LilyGO Dashboard</h1>
<form method="post" action="/api/save">
<label>SSID</label><input name="ssid" value="{}">
<label>Password</label><input name="pass" type="password" value="">
<label>Token</label><input name="token" type="password" value="">
<label>Address line</label><input name="addressLine" value="{}">
<label>City</label><input name="city" value="{}">
<label>Postal code</label><input name="postalCode" value="{}">
<label>Country</label><input name="country" value="{}">
<label>Latitude</label><input name="lat" value="{}">
<label>Longitude</label><input name="lon" value="{}">
<button type="submit">Save</button></form></body></html>)HTML",
        ssid, addressLine, city, postalCode, country, latitude, longitude);
}

std::string buildJsonSettings(const StorageManager& storage) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ssid", storage.getSSID().c_str());
    cJSON_AddStringToObject(root, "addressLine", storage.getAddressLine().c_str());
    cJSON_AddStringToObject(root, "city", storage.getCity().c_str());
    cJSON_AddStringToObject(root, "postalCode", storage.getPostalCode().c_str());
    cJSON_AddStringToObject(root, "country", storage.getCountry().c_str());
    cJSON_AddStringToObject(root, "latitude", storage.getLatitude().c_str());
    cJSON_AddStringToObject(root, "longitude", storage.getLongitude().c_str());

    char* printed = cJSON_PrintUnformatted(root);
    std::string json = printed != nullptr ? printed : "{}";
    cJSON_free(printed);
    cJSON_Delete(root);
    return json;
}

bool isWifiConnected() {
    return esp_wifi_sta_get_ap_info(nullptr) == ESP_OK;
}
}

struct WebPortal::Impl {
    httpd_handle_t server = nullptr;
    TaskHandle_t dnsTask = nullptr;
    std::string apIp = AP_IP_STR;
    bool endpointsRegistered = false;

    static esp_err_t rootHandler(httpd_req_t* request) {
        auto* portal = static_cast<WebPortal*>(request->user_ctx);
        const std::string page = buildSettingsPage(portal->storage);
        httpd_resp_set_type(request, "text/html");
        return httpd_resp_send(request, page.c_str(), static_cast<ssize_t>(page.size()));
    }

    static esp_err_t settingsHandler(httpd_req_t* request) {
        auto* portal = static_cast<WebPortal*>(request->user_ctx);
        const std::string payload = buildJsonSettings(portal->storage);
        httpd_resp_set_type(request, "application/json");
        return httpd_resp_send(request, payload.c_str(), static_cast<ssize_t>(payload.size()));
    }

    static esp_err_t saveHandler(httpd_req_t* request) {
        auto* portal = static_cast<WebPortal*>(request->user_ctx);

        std::string body;
        if (!readRequestBody(request, body)) {
            httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to read request body");
            return ESP_FAIL;
        }

        const std::string newSsid = getFormValue(body, "ssid");
        const std::string newPass = getFormValue(body, "pass");
        const std::string newToken = getFormValue(body, "token");
        const std::string newAddressLine = getFormValue(body, "addressLine");
        const std::string newCity = getFormValue(body, "city");
        const std::string newPostalCode = getFormValue(body, "postalCode");
        const std::string newCountry = getFormValue(body, "country");
        const std::string newLat = normalizeCoordinate(getFormValue(body, "lat"));
        const std::string newLon = normalizeCoordinate(getFormValue(body, "lon"));

        portal->storage.saveNetworkCredentials(newSsid, newPass);
        portal->storage.saveLocationAddress(newAddressLine, newCity, newPostalCode, newCountry);
        portal->storage.saveAppSettings(newToken, newLat, newLon);

        bool geocodedNow = false;
        if (isWifiConnected()) {
            logger::info("WiFi connected — attempting geocode now");
            geocodedNow = portal->attemptGeocodeNow();
        } else {
            logger::info("WiFi not connected — marking geocode pending");
            portal->storage.setGeocodePending(true);
        }

        const std::string response = fmt::format(R"({{"status":"ok","geocoded":{}}})", geocodedNow ? "true" : "false");
        httpd_resp_set_type(request, "application/json");
        httpd_resp_send(request, response.c_str(), static_cast<ssize_t>(response.size()));
        return ESP_OK;
    }

    static esp_err_t geocodeHandler(httpd_req_t* request) {
        auto* portal = static_cast<WebPortal*>(request->user_ctx);

        if (portal->storage.getApiToken().empty()) {
            httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "No API token saved on device. Please save settings first.");
            return ESP_OK;
        }

        if (!isWifiConnected()) {
            httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Device not connected to WiFi");
            return ESP_OK;
        }

        if (portal->attemptGeocodeNow()) {
            const std::string response = fmt::format(R"({{"status":"ok","latitude":"{}","longitude":"{}"}})", portal->storage.getLatitude(), portal->storage.getLongitude());
            httpd_resp_set_type(request, "application/json");
            httpd_resp_send(request, response.c_str(), static_cast<ssize_t>(response.size()));
        } else {
            httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Geocoding failed");
        }

        return ESP_OK;
    }

    static esp_err_t notFoundHandler(httpd_req_t* request) {
        auto* portal = static_cast<WebPortal*>(request->user_ctx);
        const std::string location = fmt::format("http://{}/", portal->impl->apIp);
        httpd_resp_set_status(request, "302 Found");
        httpd_resp_set_hdr(request, "Location", location.c_str());
        return httpd_resp_send(request, nullptr, 0);
    }

    void registerEndpoints(WebPortal* portal) {
        if (endpointsRegistered || server == nullptr) {
            return;
        }

        httpd_uri_t root{};
        root.uri = "/";
        root.method = HTTP_GET;
        root.handler = rootHandler;
        root.user_ctx = portal;
        httpd_register_uri_handler(server, &root);

        httpd_uri_t settings{};
        settings.uri = "/api/settings";
        settings.method = HTTP_GET;
        settings.handler = settingsHandler;
        settings.user_ctx = portal;
        httpd_register_uri_handler(server, &settings);

        httpd_uri_t save{};
        save.uri = "/api/save";
        save.method = HTTP_POST;
        save.handler = saveHandler;
        save.user_ctx = portal;
        httpd_register_uri_handler(server, &save);

        httpd_uri_t geocode{};
        geocode.uri = "/api/geocode";
        geocode.method = HTTP_GET;
        geocode.handler = geocodeHandler;
        geocode.user_ctx = portal;
        httpd_register_uri_handler(server, &geocode);

        httpd_uri_t fallback{};
        fallback.uri = "/*";
        fallback.method = HTTP_GET;
        fallback.handler = notFoundHandler;
        fallback.user_ctx = portal;
        httpd_register_uri_handler(server, &fallback);

        endpointsRegistered = true;
    }

    static void dnsTaskEntry(void* arg) {
        auto* impl = static_cast<Impl*>(arg);
        const int socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (socketFd < 0) {
            vTaskDelete(nullptr);
            return;
        }

        sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(DNS_PORT);
        serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(socketFd, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) != 0) {
            close(socketFd);
            vTaskDelete(nullptr);
            return;
        }

        uint8_t buffer[512];
        while (true) {
            sockaddr_in sourceAddress{};
            socklen_t sourceLength = sizeof(sourceAddress);
            const int received = recvfrom(socketFd, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&sourceAddress), &sourceLength);
            if (received <= 0 || received < 12) {
                continue;
            }

            uint8_t response[512];
            std::memcpy(response, buffer, static_cast<size_t>(received));

            response[2] = 0x81;
            response[3] = 0x80;
            response[6] = 0x00;
            response[7] = 0x01;
            response[8] = 0x00;
            response[9] = 0x00;
            response[10] = 0x00;
            response[11] = 0x00;

            size_t index = 12;
            while (index < static_cast<size_t>(received) && response[index] != 0) {
                index += static_cast<size_t>(response[index]) + 1;
            }
            index += 5;

            if (index + 16 >= sizeof(response)) {
                continue;
            }

            response[index++] = 0xC0;
            response[index++] = 0x0C;
            response[index++] = 0x00;
            response[index++] = 0x01;
            response[index++] = 0x00;
            response[index++] = 0x01;
            response[index++] = 0x00;
            response[index++] = 0x00;
            response[index++] = 0x00;
            response[index++] = 0x3C;
            response[index++] = 0x00;
            response[index++] = 0x04;

            in_addr addr{};
            inet_pton(AF_INET, impl->apIp.c_str(), &addr);
            std::memcpy(&response[index], &addr.s_addr, 4);
            index += 4;

            sendto(socketFd, response, index, 0, reinterpret_cast<sockaddr*>(&sourceAddress), sourceLength);
        }
    }
};

WebPortal::WebPortal(StorageManager& storage) : impl(std::make_unique<Impl>()), storage(storage) {}

WebPortal::~WebPortal() = default;

bool WebPortal::attemptGeocodeNow() {
    const std::string token = storage.getApiToken();
    const std::string addressLine = storage.getAddressLine();
    const std::string city = storage.getCity();
    const std::string postal = storage.getPostalCode();
    const std::string country = storage.getCountry();

    if (token.empty()) {
        logger::warn("No API token stored, cannot geocode");
        return false;
    }

    std::string url;
    bool usedZip = false;
    if (!postal.empty() && !country.empty()) {
        usedZip = true;
        url = "https://api.openweathermap.org/geo/1.0/zip?zip=" + urlEncode(postal) + "," + urlEncode(country) + "&appid=" + urlEncode(token);
    } else {
        std::string q = addressLine;
        if (!city.empty()) {
            if (!q.empty()) q += ",";
            q += city;
        }
        if (!country.empty()) {
            if (!q.empty()) q += ",";
            q += country;
        }
        url = "https://api.openweathermap.org/geo/1.0/direct?q=" + urlEncode(q) + "&limit=1&appid=" + urlEncode(token);
    }

    logger::info("Geocoding URL: {}", url.c_str());

    esp_http_client_config_t config{};
    config.url = url.c_str();
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.timeout_ms = 15000;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        logger::error("HTTP client init failed");
        return false;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        logger::error("Geocode HTTP error: {}", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    const int statusCode = esp_http_client_get_status_code(client);
    const int contentLength = esp_http_client_get_content_length(client);
    std::string payload;
    payload.reserve(contentLength > 0 ? static_cast<size_t>(contentLength) : 512);

    char buffer[256];
    int readBytes = 0;
    while ((readBytes = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
        payload.append(buffer, buffer + readBytes);
    }

    esp_http_client_cleanup(client);

    if (statusCode != 200) {
        logger::error("Geocode HTTP status: {}", statusCode);
        return false;
    }

    logger::info("Geocode payload: {}", payload.substr(0, std::min<size_t>(200, payload.size())).c_str());

    cJSON* doc = cJSON_Parse(payload.c_str());
    if (doc == nullptr) {
        logger::error("JSON parse error");
        return false;
    }

    double lat = 0.0;
    double lon = 0.0;

    if (usedZip) {
        cJSON* latNode = cJSON_GetObjectItemCaseSensitive(doc, "lat");
        cJSON* lonNode = cJSON_GetObjectItemCaseSensitive(doc, "lon");
        if (!cJSON_IsNumber(latNode) || !cJSON_IsNumber(lonNode)) {
            cJSON_Delete(doc);
            logger::warn("Zip response missing lat/lon");
            return false;
        }
        lat = latNode->valuedouble;
        lon = lonNode->valuedouble;
    } else {
        cJSON* first = cJSON_GetArrayItem(doc, 0);
        if (!cJSON_IsObject(first)) {
            cJSON_Delete(doc);
            logger::warn("Direct response not an array or empty");
            return false;
        }
        cJSON* latNode = cJSON_GetObjectItemCaseSensitive(first, "lat");
        cJSON* lonNode = cJSON_GetObjectItemCaseSensitive(first, "lon");
        if (!cJSON_IsNumber(latNode) || !cJSON_IsNumber(lonNode)) {
            cJSON_Delete(doc);
            logger::warn("Direct response missing lat/lon");
            return false;
        }
        lat = latNode->valuedouble;
        lon = lonNode->valuedouble;
    }

    cJSON_Delete(doc);

    const std::string latText = fmt::format("{:.6f}", lat);
    const std::string lonText = fmt::format("{:.6f}", lon);

    storage.saveAppSettings(storage.getApiToken(), latText, lonText);
    storage.setGeocodePending(false);

    logger::info("Geocoding saved: {}, {}", latText, lonText);
    return true;
}

void WebPortal::start() {
    logger::info("starting captive portal.");

    if (impl->server != nullptr) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&impl->server, &config) != ESP_OK) {
        logger::error("Failed to start HTTP server");
        return;
    }

    impl->registerEndpoints(this);

    if (impl->dnsTask == nullptr) {
        xTaskCreate(&Impl::dnsTaskEntry, "dns_portal", 4096, impl.get(), tskIDLE_PRIORITY + 1, &impl->dnsTask);
    }

    logger::info("Started web server and DNS server for captive portal.");
}

void WebPortal::processDNS() {
    // DNS runs in a background task in the ESP-IDF implementation.
}