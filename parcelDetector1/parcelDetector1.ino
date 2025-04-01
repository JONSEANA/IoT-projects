#include <BLEDevice.h>
#include <BLEServer.h>
#include <M5StickCPlus.h>

// Parcel detection variables
float accX = 0, accY = 0, accZ = 0;
float threshold = 2.0;
bool parcelDetected = false;
bool parcelStateSent = false; // Track if the state has already been sent
unsigned long movementStartTime = 0;

// BLE settings
#define bleServerName "ParcelG22"

unsigned long lastButtonPressTime = 0;
unsigned long buttonDebounceDelay = 200;
bool deviceConnected = false;

#define SERVICE_UUID "18b43972-c2e9-4295-b9c6-3916d8d65574"

// GYRO Characteristic and Descriptor
BLECharacteristic axpGyroCharacteristics("ec102dce-cad5-40e0-99e7-1134abf55fa1", BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor axpGyroDescriptor(BLEUUID((uint16_t)0x2903));


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


// Prototypes for painlessMesh functions
void sendParcelDetected();
// void sendMonitoring();
// void receivedCallback(uint32_t from, String & msg);
// void newConnectionCallback(uint32_t nodeId);
// void changedConnectionCallback();
// void nodeTimeAdjustedCallback(int32_t offset);
// void delayReceivedCallback(uint32_t from, int32_t delay);

// // painlessMesh and scheduler setup
// Scheduler     userScheduler;
// painlessMesh  mesh;

void setup() {
    M5.begin();
    M5.Imu.Init();
    M5.Lcd.setRotation(1);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    Serial.begin(115200);
    randomSeed(analogRead(36));

    // Initialize painlessMesh
    // mesh.setDebugMsgTypes(ERROR | DEBUG);
    // mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);
    // mesh.onReceive(&receivedCallback);
    // mesh.onNewConnection(&newConnectionCallback);
    // mesh.o