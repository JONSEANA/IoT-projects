#include <M5StickCPlus.h>
#include "BLEDevice.h"

// Parcel detection variables
float accX = 0, accY = 0, accZ = 0;
float threshold = 2.0;
bool parcelDetected = false;

// BLE settings
#define SERVICE_UUID        "17601fc7-ae60-48e3-9b40-9c240dbbe836"
#define CHARACTERISTIC_UUID "8dfcaac6-4a31-4eca-91fd-c3c37b3a202e"
#define bleServerName "THANOS"

BLEServer *pServer = nullptr;
BLEService *pService = nullptr;
BLECharacteristic *pCharacteristic = nullptr;

void setup() {
    M5.begin();
    M5.Lcd.setRotation(1);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.fillScreen(BLACK);

    Serial.begin(115200);

    // Initialize BLE Device
    BLEDevice::init(bleServerName);
    String macAddress = BLEDevice::getAddress().toString().c_str();

    pServer = BLEDevice::createServer();
    pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
                    );

    pService->start();
    pServer->getAdvertising()->start();

    // Display the MAC Address on the screen
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.printf("MAC: %s", macAddress.c_str());
    M5.Lcd.setCursor(0, 20);
    M5.Lcd.println("Monitoring...");
}

void loop() {
    M5.update();
    M5.Imu.getAccelData(&accX, &accY, &accZ);
    float totalAccel = sqrt(accX * accX + accY * accY + accZ * accZ);

    if (totalAccel > threshold && !parcelDetected) {
        parcelDetected = true;
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
        M5.Lcd.println("Parcel Detected!");
        pCharacteristic->setValue("Parcel Detected");
        pCharacteristic->notify();
        Serial.println("Parcel Detected!");
    } else if (totalAccel <= threshold && parcelDetected) {
        parcelDetected = false;
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
        M5.Lcd.println("Monitoring...");
        pCharacteristic->setValue("Monitoring");
        pCharacteristic->notify();
    }

    delay(100);
}
