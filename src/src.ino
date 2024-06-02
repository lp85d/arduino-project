#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <Servo.h>

ESP8266WebServer server(80);
WiFiClient client;
Servo servo;

const int servoPin = 0; // Используем GPIO0 для управления сервоприводом

String externalIP;
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 600000; // 10 минут в миллисекундах
unsigned long lastRequestTime = 0;
const unsigned long requestInterval = 10000; // 10 секунд в миллисекундах

void saveConfigCallback(WiFiManager *myWiFiManager) {
    // Ваш код, если необходим
}

void setup() {
    servo.attach(servoPin);
    servo.write(0); // Устанавливаем начальную позицию серво

    WiFiManager wifiManager;
    wifiManager.setAPCallback(saveConfigCallback);

    if (!wifiManager.autoConnect("ArduinoAP")) {
        delay(3000);
        ESP.restart();
        delay(5000);
    }

    server.on("/", handleRoot); // Регистрация обработчика для корневого пути
    server.begin();

    Serial.begin(115200);
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        if (millis() - lastUpdateTime > updateInterval) {
            updateExternalIP();
        }

        if (millis() - lastRequestTime > requestInterval) {
            checkServerStatus();
            lastRequestTime = millis();
        }

        server.handleClient();
    }
}

void handleRoot() {
    server.send(200, "text/html", "<h1>Выберите Wi-Fi</h1>");
}

void updateExternalIP() {
    HTTPClient http;
    http.begin(client, "https://fingerbot.ru/ip/");
    http.setTimeout(5000); // Таймаут 5 секунд
    int httpCode = http.GET();
    
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            externalIP = http.getString();
            lastUpdateTime = millis();
            Serial.println("External IP updated: " + externalIP);
        }
    } else {
        Serial.printf("Error on HTTP request: %d\n", httpCode);
    }
    
    http.end();
}

void checkServerStatus() {
    if (externalIP.isEmpty()) {
        return;
    }

    String url = "https://fingerbot.ru/wp-json/custom/v1/ip-address?custom_ip_status=" + externalIP;
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(5000); // Таймаут 5 секунд
    int httpCode = http.GET();

    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            handleServerResponse(payload);
        }
    } else {
        Serial.printf("Error on HTTP request: %d\n", httpCode);
    }

    http.end();
}

void handleServerResponse(String payload) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }

    if (doc.containsKey("code") && doc["code"] == "no_user_found") {
        Serial.println("No user found with the specified IP address. Checking again in 10 minutes.");
        lastUpdateTime = millis(); // Чтобы проверить IP через 10 минут
        return;
    }

    if (doc[0]["custom_ip_status"] == "1") {
        Serial.println("Status 1: Moving servo to 90 degrees clockwise.");
        servo.write(90); // Двигаем сервопривод на 90 градусов по часовой стрелке
    } else if (doc[0]["custom_ip_status"] == "0") {
        Serial.println("Status 0: Moving servo to 90 degrees counterclockwise.");
        servo.write(-90); // Двигаем сервопривод на 90 градусов против часовой стрелки
    }
}
