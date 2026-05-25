#include "CaptivePortal.h"

namespace {
    constexpr byte DNS_PORT = 53;
}

CaptivePortal::CaptivePortal(StorageManager& storage) : storage(storage), server(80) {}

void CaptivePortal::start(IPAddress apIP) {
    Serial.println("[CaptivePortal] starting captive portal.");

    dnsServer.start(DNS_PORT, "*", apIP);


    setupEndpoints();
    server.begin();

    Serial.println("[CaptivePortal] Started web server and DNS server for captive portal.");
}

void CaptivePortal::processDNS() {
    dnsServer.processNextRequest();
}

void CaptivePortal::setupEndpoints() {

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", "<h1>test</h1>");
    });


    server.on("/api/save", HTTP_POST, [this](AsyncWebServerRequest *request){
        String newSsid = "", newPass = "", newToken = "", newLat = "", newLon = "";

        if (request->hasParam("ssid", true)) newSsid = request->getParam("ssid", true)->value();
        if (request->hasParam("pass", true)) newPass = request->getParam("pass", true)->value();
        if (request->hasParam("token", true)) newToken = request->getParam("token", true)->value();
        if (request->hasParam("lat", true)) newLat = request->getParam("lat", true)->value();
        if (request->hasParam("lon", true)) newLon = request->getParam("lon", true)->value();


        this->storage.saveNetworkCredentials(newSsid, newPass);
        this->storage.saveAppSettings(newToken, newLat, newLon);

        request->send(200, "text/html", "<html><body style='font-family:sans-serif; text-align:center; margin-top:50px;'><h2>Zapisano pomyślnie!</h2><p>Trwa restart urzadzenia...</p></body></html>");
        

        delay(1000); 
        ESP.restart();
    });

    server.onNotFound([](AsyncWebServerRequest *request){

        request->redirect("http://192.168.4.1/"); // todo make it automatic
    });
}