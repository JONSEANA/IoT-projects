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
    // mesh.onChangedConnections(&changedConnectionCallback);
    // mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
    // mesh.onNodeDelayReceived(&delayReceivedCallback);

    // Configure HOME button as input
    pinMode(M5_BUTTON_HOME, INPUT);

    // Create the BLE Device
    BLEDevice::init(bleServerName);

    // Create the BLE Server
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
  BLEService *bleService = pServer->createService(SERVICE_UUID);

     // Battery
  bleService->addCharacteristic(&axpGyroCharacteristics);
  axpGyroDescriptor.setValue("axpGyro");
  axpGyroCharacteristics.addDescriptor(&axpGyroDescriptor);

    // Display initial "Monitoring..." message on the screen
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.printf("Monitoring...\n");
    parcelStateSent = false; // Reset state

    //Start the service
    bleService->start();

    // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

}

void loop() {
    // Parcel detection logic
    M5.Imu.getAccelData(&accX, &accY, &accZ);
    float totalAccel = sqrt(accX * accX + accY * accY + accZ * accZ);

    if (totalAccel > threshold) {
        if (!parcelDetected) {
            // Parcel detected, update the state
            parcelDetected = true;
            movementStartTime = millis();
            
            // Display "Parcel Detected!" on the screen and send message
            M5.Lcd.fillScreen(TFT_BLACK);
            M5.Lcd.setCursor(0, 0);
            M5.Lcd.printf("Parcel Detected!\n");
            Serial.println("Parcel Detected!");
                        // Send the parcel detected message only once
            sendParcelDetected();
        }
    } else {
        if (parcelDetected && millis() - movementStartTime >= 5000) {
            // After 5 seconds of no detection, switch back to monitoring
            parcelDetected = false;
            M5.Lcd.fillScreen(TFT_BLACK);
            M5.Lcd.setCursor(0, 0);
            M5.Lcd.printf("Monitoring...\n");
        }

        if (!parcelDetected && !parcelStateSent) {
            // If not detected and message hasn't been sent yet, send the monitoring message
            // sendMonitoring();
            parcelStateSent = true; // Ensure the monitoring message is only sent once
        }
    }

    // mesh.update();  // Update the mesh network
    delay(100);
}

void sendParcelDetected() {
    String msg = "Parcel detected from node: ";
    // msg += mesh.getNodeId();

    axpGyroCharacteristics.setValue("1");
    axpGyroCharacteristics.notify();
    Serial.printf("Sending message: %s\n", msg.c_str());

    // Reset the parcelStateSent flag so that the monitoring message can be sent next time
    parcelStateSent = false;
}

// void sendMonitoring() {
//     String msg = "Monitoring from node: ";
//     msg += mesh.getNodeId();
//     mesh.sendBroadcast(msg);
//     Serial.printf("Sending monitoring message: %s\n", msg.c_str());
// }

// void receivedCallback(uint32_t from, String & msg) {
//     Serial.printf("Received from %u msg=%s\n", from, msg.c_str());

//     // Display received message on M5StickC screen
//     M5.Lcd.fillScreen(TFT_BLACK);
//     M5.Lcd.setCursor(0, 10);
//     M5.Lcd.setTextSize(1);
//     M5.Lcd.setTextColor(WHITE, BLACK);
//     M5.Lcd.print("Received from node: ");
//     M5.Lcd.println(from);
//     M5.Lcd.print("Message: ");
//     M5.Lcd.println(msg);

// }

// void newConnectionCallback(uint32_t nodeId) {
//     Serial.printf("New Connection, nodeId = %u\n", nodeId);
// }

// void changedConnectionCallback() {
//     Serial.printf("Changed connections\n");
// }

// void nodeTimeAdjustedCallback(int32_t offset) {
//     Serial.printf("Adjusted time. Offset = %d\n", offset);
// }

// void delayReceivedCallback(uint32_t from, int32_t delay) {
//     Serial.printf("Delay to node %u is %d us\n", from, delay);
// }