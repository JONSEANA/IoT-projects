#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <M5StickCPlus.h>

// Parcel detection variables
float accX = 0, accY = 0, accZ = 0;
float threshold = 2.0;
bool parcelDetected = false;
unsigned long movementStartTime = 0;

// BLE settings
#define bleServerName "ParcelG22"
#define motionServerName "ParcelMotion"
bool doConnect = false;
bool deviceConnected = false;
bool bleConnected = false;

// UUIDs
#define SERVICE_UUID "18b43972-c2e9-4295-b9c6-3916d8d65574"
#define MOTION_UUID "381423c5-088e-4230-bb52-af5f65b282e5"

// Characteristics
BLECharacteristic axpGyroCharacteristics("ec102dce-cad5-40e0-99e7-1134abf55fa1", 
                                        BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor gyroDescriptor(BLEUUID((uint16_t)0x2902));

BLECharacteristic axpMotionCharacteristics(MOTION_UUID, 
                                          BLECharacteristic::PROPERTY_NOTIFY | 
                                          BLECharacteristic::PROPERTY_READ | 
                                          BLECharacteristic::PROPERTY_WRITE);

static BLEUUID motionServerUUID("18b43972-c2e9-4295-b9c6-3916d8d65369");
static BLEUUID motionCharacteristicUUID(MOTION_UUID);
static BLERemoteCharacteristic* motionCharacteristic;
static BLEAddress* pServerAddress;
static BLEClient* pClient = nullptr;

// Server Callbacks
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("[SERVER] Client connected");
  }
  
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("[SERVER] Client disconnected");
    pServer->startAdvertising();
    Serial.println("[SERVER] Restarted advertising");
  }
};

// Client Callbacks
class MyClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("[CLIENT] Connected to server");
    bleConnected = true;
  }

  void onDisconnect(BLEClient* pclient) {
    Serial.println("[CLIENT] Disconnected from server");
    bleConnected = false;
    doConnect = true;
  }
};

// Motion Control Callbacks
class MotionControlCallback : public BLECharacteristicCallbacks { 
  void onWrite(BLECharacteristic *pCharacteristic) { 
    std::string value = pCharacteristic->getValue();
    Serial.printf("[MOTION] Received command: %s\n", value.c_str());
    
    if (value == "1") { 
      digitalWrite(M5_LED, LOW); 
      Serial.println("[MOTION] Detection enabled");
    } else if (value == "0") { 
      digitalWrite(M5_LED, HIGH); 
      Serial.println("[MOTION] Detection disabled");
    } 
  } 
};

// Scan Callbacks
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks { 
  void onResult(BLEAdvertisedDevice advertisedDevice) { 
    Serial.printf("[SCAN] Found device: %s\n", advertisedDevice.toString().c_str());
    
    if (advertisedDevice.getName() == motionServerName) {
      Serial.println("[SCAN] Target server found!");
      advertisedDevice.getScan()->stop();
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;
    }
  } 
};

bool connectToServer(BLEAddress pAddress) { 
  Serial.printf("[CONNECT] Attempting connection to %s\n", pAddress.toString().c_str());
  
  if(pClient == nullptr) {
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallbacks());
  }

  if (!pClient->connect(pAddress)) {
    Serial.println("[CONNECT] Connection failed");
    delete pClient;
    pClient = nullptr;
    return false;
  }

  BLERemoteService* pRemoteService = pClient->getService(motionServerUUID);
  if (pRemoteService == nullptr) {
    Serial.println("[CONNECT] Service not found");
    pClient->disconnect();
    return false;
  }

  motionCharacteristic = pRemoteService->getCharacteristic(motionCharacteristicUUID);
  if (motionCharacteristic == nullptr) {
    Serial.println("[CONNECT] Characteristic not found");
    pClient->disconnect();
    return false;
  }

  Serial.println("[CONNECT] Connection successful");
  return true;
}

void setup() {
  M5.begin();
  M5.Imu.Init();
  Serial.begin(115200);
  Serial.println("\n[M5] Initializing system...");

  // BLE Initialization
  BLEDevice::init(bleServerName);
  Serial.println("[BLE] Device initialized");

  // Server Setup
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *bleService = pServer->createService(SERVICE_UUID);
  
  // Add characteristics
  bleService->addCharacteristic(&axpGyroCharacteristics);
  axpGyroCharacteristics.addDescriptor(&gyroDescriptor);
  
  bleService->addCharacteristic(&axpMotionCharacteristics);
  axpMotionCharacteristics.setCallbacks(new MotionControlCallback());
  
  bleService->start();
  
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  Serial.println("[SERVER] Advertising started");

  // Display Setup
  M5.Lcd.setRotation(1);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("Initializing...");

  // Start BLE Scan
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->start(5, false);
  Serial.println("[SCAN] Started scanning");
}

void loop() {
  // Handle BLE connections
  if (doConnect) {
    if(connectToServer(*pServerAddress)) {
      doConnect = false;
      M5.Lcd.fillScreen(TFT_BLACK);
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.print("Connected");
    } else {
      Serial.println("[CONNECT] Retrying scan...");
      BLEDevice::getScan()->start(5, false);
      doConnect = false;
    }
  }

  // Motion detection logic
  M5.Imu.getAccelData(&accX, &accY, &accZ);
  float totalAccel = sqrt(accX*accX + accY*accY + accZ*accZ);
  Serial.printf("[SENSOR] Acceleration: %.2f\n", totalAccel);

  if (totalAccel > threshold && !parcelDetected) {
    parcelDetected = true;
    movementStartTime = millis();
    Serial.println("[DETECTION] Movement detected!");
    
    M5.Lcd.fillScreen(TFT_RED);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.print("MOVEMENT!");
    
    if(bleConnected && motionCharacteristic != nullptr) {
      axpGyroCharacteristics.setValue("1");
      axpGyroCharacteristics.notify();
      Serial.println("[NOTIFY] Sent detection alert");
    }
  }
  else if (parcelDetected && (millis() - movementStartTime >= 5000)) {
    parcelDetected = false;
    Serial.println("[DETECTION] Movement cleared");
    
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.print("Monitoring...");
    
    if(bleConnected && motionCharacteristic != nullptr) {
      axpGyroCharacteristics.setValue("0");
      axpGyroCharacteristics.notify();
      Serial.println("[NOTIFY] Sent clear notification");
    }
  }

  delay(250); // Reduced delay for better responsiveness
}
