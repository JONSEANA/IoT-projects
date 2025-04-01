#include <M5StickCPlus.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// Define UUIDs for BLE service and characteristics
#define SERVICE_UUID        "1101"
#define CHARACTERISTIC_UUID "2101"
#define bleServerName "THANOS" // Server Name

bool isLocked = true;

// Define pointers for BLE characteristics and service
BLEServer *pServer = nullptr;
BLEService *pService = nullptr;
BLECharacteristic *pCharacteristic = nullptr;

void setup() {
  M5.begin();
  pinMode(10, OUTPUT);
  digitalWrite(10, LOW); // Initial locked state (LED off)
  
  // Create the BLE Device
  BLEDevice::init(bleServerName); // 'THANOS' as the device name
  String macAddress = BLEDevice::getAddress().toString().c_str();
  
  // Create the BLE Server
  pServer = BLEDevice::createServer();
  
  // Create the BLE Service
  pService = pServer->createService(SERVICE_UUID);
  
  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ | 
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  
  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();

  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.drawString(isLocked ? "Lock: LOCKED" : "Lock: UNLOCKED", 10, 10, 2);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("MAC: %s", macAddress.c_str());
  M5.Lcd.setCursor(0, 20);

  // Initial value for characteristic
  pCharacteristic->setValue(isLocked ? "Locked" : "Unlocked");
}

void loop() {
  M5.update(); // Update button states

  if (M5.BtnA.wasPressed()) {
    isLocked = !isLocked; // Toggle lock state
    digitalWrite(10, isLocked ? LOW : HIGH); // Update LED based on lock state
    M5.Lcd.fillRect(0, 0, 160, 80, BLACK);
    M5.Lcd.drawString(isLocked ? "Lock: LOCKED" : "Lock: UNLOCKED", 10, 10, 2);
    pCharacteristic->setValue(isLocked ? "Locked" : "Unlocked");
    pCharacteristic->notify(); // Notify connected client about the change
  }

  delay(100); // Delay to reduce CPU usage
}
