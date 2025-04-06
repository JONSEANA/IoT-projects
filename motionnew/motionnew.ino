#include <M5StickCPlus.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include "BLEDevice.h"

const int pirPin = 33; // PIR sensor connected to GPIO 33
bool objectDetected = false;
unsigned long lastDetectionTime = 0;
const unsigned long debounceTime = 500; // Minimum time between detections (ms)
const unsigned long sustainTime = 1000;
bool deviceConnected = false;
bool doConnect = false;
bool connected = false;
volatile bool isLocked = false;

// BLE Config
#define bleMotionServerName "ParcelMotion"
#define lockServerName "ParcelLock"
#define SERVICE_UUID "18b43972-c2e9-4295-b9c6-3916d8d65369"
#define MOTION_UUID "381423c5-088e-4230-bb52-af5f65b282e5"
#define LOCK_UUID "381423c5-088e-4230-bb52-af5f65b283b1"

// Create characteristics for motion
BLECharacteristic motionControlCharacteristic(MOTION_UUID, 
                                             BLECharacteristic::PROPERTY_READ | 
                                             BLECharacteristic::PROPERTY_WRITE | 
                                             BLECharacteristic::PROPERTY_NOTIFY);

// Address of the peripheral device. Address will be found during scanning...
static BLEUUID lockServiceUUID("18b43972-c2e9-4295-b9c6-3916d8d65456");
static BLEUUID lockCharacteristicUUID(LOCK_UUID);
static BLERemoteCharacteristic* lockCharacteristic;
static BLEAddress* pServerAddress;
static BLEClient* pClient = NULL;

// Notification callback function for lock characteristic
// Update the notification callback to clear detection state when unlocked
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                          uint8_t* pData, size_t length, bool isNotify) {
  String value = String((char*)pData, length);
  Serial.print("Notification received from lock server: ");
  Serial.println(value);
  
  if (value == "locked") {
    digitalWrite(M5_LED, LOW);
    isLocked = true;
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(10, 20);
    M5.Lcd.setTextColor(RED);
    M5.Lcd.println("LOCKED");
  } 
  else if (value == "unlocked") {
    digitalWrite(M5_LED, HIGH);
    isLocked = false;
    
    // Clear detection state
    objectDetected = false;
    lastDetectionTime = 0;
    
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(10, 20);
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.println("UNLOCKED");
    
    // Send final "clear" notification
    try {
      motionControlCharacteristic.setValue("0");
      motionControlCharacteristic.notify();
    } catch (...) {
      Serial.println("Final clear notification failed");
    }
  }
}

// Setup callbacks onConnect and onDisconnect for our motion server
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("Motion server: Client connected");
  };
  
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("Motion server: Client disconnected");
    delay(500);
    pServer->startAdvertising();
    Serial.println("Motion server: Advertising restarted");
  }
};

class MOTIONControlCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      if (value == "1") {
        digitalWrite(M5_LED, HIGH);
        Serial.println("Motion detected signal sent to client");
      } else if (value == "0") {
        digitalWrite(M5_LED, LOW);
        Serial.println("Motion cleared signal sent to client");
      }
    }
  }
};

// Scan for lock server
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("Found device: ");
    Serial.println(advertisedDevice.getName().c_str());
    
    if (advertisedDevice.getName() == lockServerName) {
      Serial.println("Found our lock server!");
      advertisedDevice.getScan()->stop();
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;
    }
  }
};

// Client callbacks for tracking connection state
class MyClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    connected = true;
    Serial.println("Connected to lock server");
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("Disconnected from lock server");
    doConnect = true; // Try to reconnect
  }
};

bool connectToLockServer() {
  Serial.print("Connecting to ");
  Serial.println(pServerAddress->toString().c_str());
  
  // Create client if needed
  if (pClient == NULL) {
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallbacks());
  }
  
  // Connect to the remote BLE Server
  if (!pClient->connect(*pServerAddress)) {
    Serial.println("Connection failed");
    return false;
  }
  
  // Obtain a reference to the lock service
  BLERemoteService* pRemoteService = pClient->getService(lockServiceUUID);
  if (pRemoteService == nullptr) {
    Serial.println("Failed to find lock service UUID");
    pClient->disconnect();
    return false;
  }
  
  // Obtain a reference to the lock characteristic
  lockCharacteristic = pRemoteService->getCharacteristic(lockCharacteristicUUID);
  if (lockCharacteristic == nullptr) {
    Serial.println("Failed to find lock characteristic UUID");
    pClient->disconnect();
    return false;
  }
  
  // Read the initial value
  if (lockCharacteristic->canRead()) {
    std::string value = lockCharacteristic->readValue();
    Serial.print("Initial lock value: ");
    Serial.println(value.c_str());
    
    if (value == "locked") {
      isLocked = true;
      digitalWrite(M5_LED, LOW);
    } else if (value == "unlocked") {
      isLocked = false;
      digitalWrite(M5_LED, HIGH);
    }
  }
  
  // Register for notifications
  if (lockCharacteristic->canNotify()) {
    lockCharacteristic->registerForNotify(notifyCallback);
    Serial.println("Registered for lock notifications");
  } else {
    Serial.println("Lock characteristic cannot notify");
  }
  
  return true;
}

void setup() {
  M5.begin();
  Serial.begin(115200);
  pinMode(pirPin, INPUT);
  
  // Create the BLE Device
  BLEDevice::init(bleMotionServerName);
  
  // Create the BLE Server for motion detection
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *bleService = pServer->createService(SERVICE_UUID);
  bleService->addCharacteristic(&motionControlCharacteristic);
  motionControlCharacteristic.setValue("0"); // Initial state is OFF
  motionControlCharacteristic.setCallbacks(new MOTIONControlCallback());
  bleService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();
  Serial.println("Motion server started. Waiting for client connections...");
  
  // Start scanning for lock server
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(10, false); // Start scan for 10 seconds, non-blocking
  
  // LCD Setup
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 20);
  M5.Lcd.println("Scanning for lock...");
}

void loop() {
  static unsigned long highStartTime = 0;
  static bool sustainedDetection = false;
  int currentPirState = digitalRead(pirPin);
  
  // Handle lock server connection
  if (doConnect) {
    if (connectToLockServer()) {
      Serial.println("Connected to lock server successfully");
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(10, 20);
      M5.Lcd.setTextColor(GREEN);
      M5.Lcd.println(isLocked ? "LOCKED" : "UNLOCKED");
    } else {
      Serial.println("Failed to connect to lock server");
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(10, 20);
      M5.Lcd.setTextColor(RED);
      M5.Lcd.println("LOCK CONNECTION FAILED");
      
      // Restart scanning in 5 seconds
      delay(5000);
      BLEScan* pBLEScan = BLEDevice::getScan();
      pBLEScan->start(10, false);
    }
    doConnect = false;
  }
  
  // Advanced PIR Detection Logic
  if (currentPirState == HIGH && isLocked) {
    if (highStartTime == 0) {
      highStartTime = millis();
    } else if (!sustainedDetection && (millis() - highStartTime >= sustainTime)) {
      sustainedDetection = true;
    }
  } else {
    highStartTime = 0;
    sustainedDetection = false;
  }
  
  // Detection Processing with Debouncing
  if (sustainedDetection && !objectDetected && (millis() - lastDetectionTime >= debounceTime)) {
    handleDetection(true);
    lastDetectionTime = millis();
    objectDetected = true;
  } else if (!sustainedDetection && objectDetected) {
    handleDetection(false);
    objectDetected = false;
  }
  
  // If we're supposed to be connected but aren't, try reconnecting
  if (!connected && !doConnect) {
    Serial.println("Connection lost, will try to reconnect...");
    doConnect = true;
  }
  
  delay(50);
}

void handleDetection(bool detected) {
  Serial.println(detected ? "MOTION: Object Detected!" : "MOTION: Clear");
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(10, 20);
  M5.Lcd.setTextColor(detected ? RED : GREEN);
  M5.Lcd.println(detected ? "Object Detected!" : isLocked ? "LOCKED" : "UNLOCKED");
  
  // Only send notifications when LOCKED
  if(isLocked) {  // <--- This is the crucial change
    try {
      motionControlCharacteristic.setValue(detected ? "1" : "0");
      motionControlCharacteristic.notify();
      Serial.println(detected ? "Motion notification: TRUE" : "Motion notification: FALSE");
    } catch (...) {
      Serial.println("BLE notification error");
    }
  }
}