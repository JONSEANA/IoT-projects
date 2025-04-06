#include <M5StickCPlus.h>
#include <BLEServer.h>
#include "BLEDevice.h"

bool isLocked = true;


bool deviceConnected =false;
bool doConnect = false;
// BLE Config
#define bleLockServerName "ParcelLock"

#define SERVICE_UUID "18b43972-c2e9-4295-b9c6-3916d8d65456"
#define LOCK_UUID "381423c5-088e-4230-bb52-af5f65b283b1"

//create characteristics for motion
BLECharacteristic lockControlCharacteristic(LOCK_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);

//Address of the peripheral device. Address will be found during scanning...
static BLEAddress* pServerAddress;

//Setup callbacks onConnect and onDisconnect
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("MyServerCallbacks::Connected...");
  };
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("MyServerCallbacks::Disconnected...");
    delay(500);                   
    pServer->startAdvertising();  
    Serial.println("Advertising restarted");
  }
};

class LockControlCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0) {
      if (value == "1") {
        digitalWrite(M5_LED, 1);  
        Serial.println("LED turned ON by client.");
      } else if (value == "0") {
        digitalWrite(M5_LED, 0);  
        Serial.println("LED turned OFF by client.");
      }
    }
  }
};

void setup() {
  M5.begin();
  Serial.begin(115200);


  // Create the BLE Device
  BLEDevice::init(bleLockServerName);

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *bleService = pServer->createService(SERVICE_UUID);

  //motion characteristics creation
  bleService->addCharacteristic(&lockControlCharacteristic);
  lockControlCharacteristic.setValue("0");  // Initial state is OFF
  lockControlCharacteristic.setCallbacks(new LockControlCallback());

  // Start the service
  bleService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE);
}

void loop() {

  // Advanced PIR Detection Logic
   M5.update(); // Update button states

  // Check if button A (front button) is pressed
  if (M5.BtnA.wasPressed()) {
    isLocked = !isLocked; // Toggle lock state
    
    // Update LED and display
    digitalWrite(10, isLocked ? LOW : HIGH);
    M5.Lcd.fillRect(0, 0, 160, 80, BLACK);
    M5.Lcd.drawString(isLocked ? "Lock: LOCKED" : "Lock: UNLOCKED", 10, 10, 2);

    sendLockStatus();
  }

  delay(50);
}

void sendLockStatus() {
    String msg = isLocked ? "locked" : "unlocked";

    Serial.printf("Sending message: %s\n", msg.c_str());

    try {
      lockControlCharacteristic.setValue(msg.c_str());
      lockControlCharacteristic.notify();
    } catch (...) {
      Serial.println("BLE Write Error");
    }
}

