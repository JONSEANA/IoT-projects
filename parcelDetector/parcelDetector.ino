#include <M5StickCPlus.h>
#include <WiFi.h>
#include <PubSubClient.h>

// WiFi and MQTT Configuration
const char* ssid = "AndroidAP284C";
const char* password = "cufj4615";
const char* mqtt_server = "192.168.59.37";
const int mqtt_port = 8883;
const char* mqtt_user = "jiamin";
const char* mqtt_password = "jiamin";
const char* mqtt_topic = "parcel/status";

WiFiClient espClient;
PubSubClient client(espClient);

// Parcel detection variables
float accX = 0, accY = 0, accZ = 0;
float threshold = 2.0;
bool parcelDetected = false;
unsigned long movementStartTime = 0;

void setup() {
    M5.begin();
    Serial.begin(115200);
    M5.Lcd.setRotation(1);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE, BLACK);

    setupWifi();
    client.setServer(mqtt_server, mqtt_port);

    M5.Imu.Init();

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 10);
    M5.Lcd.println("Monitoring...");
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    // Parcel detection logic
    M5.Imu.getAccelData(&accX, &accY, &accZ);
    float totalAccel = sqrt(accX * accX + accY * accY + accZ * accZ);

    if (totalAccel > threshold && !parcelDetected) {
        parcelDetected = true;
        movementStartTime = millis();

        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 10);
        M5.Lcd.println("Parcel Detected!");

        String msg = "Parcel Detected!";
        client.publish(mqtt_topic, msg.c_str());
    } else if (parcelDetected && millis() - movementStartTime >= 5000) {
        parcelDetected = false;
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 10);
        M5.Lcd.println("Monitoring...");
    }
}

void setupWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        M5.Lcd.print(".");
    }
    M5.Lcd.println("\nWiFi Connected");
    M5.Lcd.println("IP Address: ");
    M5.Lcd.println(WiFi.localIP());
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (client.connect("M5StickClient", mqtt_user, mqtt_password)) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}
