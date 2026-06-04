#include "TimeManager.h"
#include <ctime>
#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_wifi.h>
#include <lwip/apps/sntp.h>
#include <fmt/format.h>
#include <cJSON.h>
#include "Log.h"

TimeManager::TimeManager(StorageManager& storage) : storage(storage) {}

namespace {
    void applyTimezoneOffsetSeconds(int32_t offsetSeconds) {
        if (offsetSeconds == 0) {
            setenv("TZ", "UTC0", 1);
            tzset();
            return;
        } else {
            char sign = offsetSeconds >= 0 ? '-' : '+';
            int32_t absoluteOffset = abs(offsetSeconds);
            int hours = absoluteOffset / 3600;
            int minutes = (absoluteOffset % 3600) / 60;
            const std::string tzBuffer = minutes == 0
                ? fmt::format("UTC{}{}", sign, hours)
                : fmt::format("UTC{}{}:{:02}", sign, hours, minutes);
            setenv("TZ", tzBuffer.c_str(), 1);
            tzset();
            return;
        }
    }

    bool isWifiConnected() {
        return esp_wifi_sta_get_ap_info(nullptr) == ESP_OK;
    }
}

void TimeManager::begin() {
    if (ntpStarted) return;
    applyTimezoneOffsetSeconds(storage.getTimezoneOffsetSeconds());
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(1, "time.nist.gov");
    sntp_init();
    ntpStarted = true;
    logger::info("NTP started");
}

long TimeManager::fetchTimezoneOffsetSeconds(double lat, double lon) {
    int32_t cachedOffset = storage.getTimezoneOffsetSeconds();
    const std::string token = storage.getApiToken();
    if (token.empty()) {
        logger::warn("No API token available for timezone lookup, using cached offset if available");
        return cachedOffset;
    }

    if (!isWifiConnected()) {
        logger::warn("WiFi not connected, using cached timezone offset");
        return cachedOffset;
    }

    const std::string urlBuf = fmt::format(
        "https://api.openweathermap.org/data/2.5/weather?lat={}&lon={}&appid={}&units=metric",
        lat,
        lon,
        token.c_str());

    esp_http_client_config_t config{};
    config.url = urlBuf.c_str();
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.timeout_ms = 15000;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        logger::error("Timezone HTTP client init failed");
        return cachedOffset;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        logger::error("Timezone HTTP request failed: {}", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return cachedOffset;
    }

    const int statusCode = esp_http_client_get_status_code(client);
    std::string payload;
    char buffer[256];
    int readBytes = 0;
    while ((readBytes = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
        payload.append(buffer, buffer + readBytes);
    }

    esp_http_client_cleanup(client);

    if (statusCode != 200) {
        logger::error("Geocode HTTP status: {}", statusCode);
        return cachedOffset;
    }

    cJSON* doc = cJSON_Parse(payload.c_str());
    if (doc == nullptr) {
        logger::error("JSON parse error");
        return cachedOffset;
    }

    cJSON* tzNode = cJSON_GetObjectItemCaseSensitive(doc, "timezone");
    if (!cJSON_IsNumber(tzNode)) {
        cJSON_Delete(doc);
        logger::warn("Timezone field not found in response");
        return cachedOffset;
    }

    long tz = static_cast<long>(tzNode->valuedouble);
    logger::info("Fetched timezone={}", tz);
    storage.saveTimezoneOffsetSeconds(static_cast<int32_t>(tz));
    applyTimezoneOffsetSeconds(static_cast<int32_t>(tz));
    cJSON_Delete(doc);
    return tz;
}

std::string TimeManager::getLocalTimeString(double lat, double lon) {
    if (!ntpStarted) begin();

    struct tm localNow {};
    const time_t now = time(nullptr);
    if (now < 1700000000 || !localtime_r(&now, &localNow)) {
        logger::warn("NTP returned an invalid epoch");
        return {};
    }

    long tzOffset = storage.getTimezoneOffsetSeconds();
    if (lat != 0.0 || lon != 0.0) {
        tzOffset = fetchTimezoneOffsetSeconds(lat, lon);
    }

    (void)tzOffset;

    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &localNow);
    return std::string(buf);
}
