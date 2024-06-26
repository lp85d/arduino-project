#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <Servo.h>

ESP8266WebServer server(80);
WiFiClient client;
Servo servo;

const int servoPin = 0;
String externalIP;
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 24 * 60 * 60 * 1000; // Интервал обновления IP (24 часа)
unsigned long lastRequestTime = 0;
const unsigned long requestInterval = 10000; // Интервал проверки статуса сервера (10 секунд)
int currentPosition = 90; // Начальное положение сервопривода (90 градусов для середины диапазона)
String currentStatus = "Unknown";

void saveConfigCallback(WiFiManager *myWiFiManager) {}

void setup() {
    Serial.begin(115200);
    servo.attach(servoPin);
    servo.write(currentPosition);

    WiFiManager wifiManager;
    wifiManager.setAPCallback(saveConfigCallback);
    wifiManager.setConfigPortalTimeout(60); // Уменьшено время тайм-аута для соединения

    if (!wifiManager.autoConnect()) {
        wifiManager.startConfigPortal(); // Попытка подключения к доступной сети
        delay(3000);
        ESP.restart();
    }

    server.on("/", handleRoot);
    server.begin();

    if (WiFi.status() == WL_CONNECTED) {
        updateExternalIP();
    }

    lastUpdateTime = millis();
    lastRequestTime = millis();
}

void loop() {
    unsigned long currentMillis = millis();
    
    if (WiFi.status() == WL_CONNECTED) {
        if (currentMillis - lastUpdateTime > updateInterval) {
            updateExternalIP();
            lastUpdateTime = currentMillis;
        }

        if (currentMillis - lastRequestTime > requestInterval) {
            checkServerStatus();
            lastRequestTime = currentMillis;
        }
        
        server.handleClient();
    } else {
        if (currentMillis - lastRequestTime > requestInterval) {
            reconnectWiFi();
            lastRequestTime = currentMillis;
        }
    }
}

void handleRoot() {
    String message = "<h1>Device Status</h1>";
    message += "<p>Current Position: " + String(currentPosition) + " degrees</p>";
    message += "<p>Current Status: " + currentStatus + "</p>";
    message += "<p>Signal Strength: " + String(WiFi.RSSI()) + " dBm</p>";
    message += "<p>External IP: " + externalIP + "</p>";

    server.send(200, "text/html", message);
}

void updateExternalIP() {
    if (WiFi.status() == WL_CONNECTED) {
        sendHttpRequest("http://fingerbot.ru/ip/", [&](int httpCode, const String& payload) {
            if (httpCode == HTTP_CODE_OK) {
                externalIP = payload;
            }
        });
    }
}

void checkServerStatus() {
    if (externalIP.isEmpty()) {
        return;
    }

    String url = "http://fingerbot.ru/wp-json/custom/v1/ip-address?custom_ip_status=" + externalIP;
    sendHttpRequest(url, [&](int httpCode, const String& payload) {
        if (httpCode == HTTP_CODE_OK) {
            handleServerResponse(payload);
        }
    });
}

void handleServerResponse(const String& response) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        return;
    }

    int status = doc[0]["custom_ip_status"].as<int>();

    if (status == 1 && currentStatus != "1") {
        currentPosition = 180;
        servo.write(currentPosition);
        currentStatus = "1";
    } else if (status == 0 && currentStatus != "0") {
        currentPosition = 0;
        servo.write(currentPosition);
        currentStatus = "0";
    }
}

void sendHttpRequest(const String& url, std::function<void(int, const String&)> callback) {
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(5000);
    int httpCode = http.GET();
    String payload = httpCode > 0 ? http.getString() : "";

    callback(httpCode, payload);
    http.end();
}

void reconnectWiFi() {
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(60);
    if (!wifiManager.autoConnect("FingerBot")) {
        delay(3000);
        ESP.restart();
    }
}
