#define _GLIBCXX_HAVE_BROKEN_VSWPRINTF 1
#undef min
#undef max

#include <queue>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include "BLEDevice.h"
#include <M5StickCPlus.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// Wi-Fi/MQTT Config
const char* ssid = "AndroidAP284C";
const char* password = "cufj4615";
const char* mqttBroker = "test.mosquitto.org";
const int mqttPort = 1883;

AsyncMqttClient mqttClient;

// BLE Config (unchanged from your original code)
#define bleServerName "ParcelG22"
BLEScan* pBLEScan;
BLEAdvertisedDevice* targetDevice;

static BLEUUID bleServiceUUID("18b43972-c2e9-4295-b9c6-3916d8d65574");
static BLEUUID gyroCharacteristicUUID("ec102dce-cad5-40e0-99e7-1134abf55fa1");

static boolean doConnect = false;
static boolean Connected = false;
static BLEAddress* pServerAddress;
static BLERemoteCharacteristic* gyroCharacteristic;

// Gyro data variables
char gyroBuffer[20] = {0};
bool newGyro = false;

// Display variables
const int MAX_SCREEN_LINES = 7;
String screenBuffer[MAX_SCREEN_LINES];
int currentScreenLine = 0;

// Message queue for QoS 1
std::queue<String> messageQueue;

// Custom debug output
void debugPrintln(String message) {
  Serial.println(message);
  
  // Update screen buffer
  screenBuffer[currentScreenLine] = message;
  currentScreenLine = (currentScreenLine + 1) % MAX_SCREEN_LINES;
  
  // Update display
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  for(int i = 0; i < MAX_SCREEN_LINES; i++) {
    int lineIndex = (currentScreenLine + i) % MAX_SCREEN_LINES;
    M5.Lcd.println(screenBuffer[lineIndex]);
  }
}

// BLE Notification Callback (unchanged)
static void gyroNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                               uint8_t* pData, size_t length, bool isNotify) {
  if (length < sizeof(gyroBuffer)) {
    memset(gyroBuffer, 0, sizeof(gyroBuffer));
    strncpy(gyroBuffer, (char*)pData, length);
    newGyro = true;
    debugPrintln("BLE Received: " + String(gyroBuffer));
  }
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == bleServerName) {
      advertisedDevice.getScan()->stop();
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;
      debugPrintln("Device found! Connecting...");
    }
  }
};

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {}
  
  void onDisconnect(BLEClient* pclient) {
    Connected = false;
    debugPrintln("BLE Disconnected");
    BLEDevice::deinit(true);
    delay(500);
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5);
  }
};

// MQTT Event Handlers
void onMqttConnect(bool sessionPresent) {
  debugPrintln("MQTT Connected");
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  debugPrintln("MQTT Disconnected");
}

void onMqttPublish(uint16_t packetId) {
  debugPrintln("Publish acknowledged (QoS 1)");
}

void connectToMqtt() {
  debugPrintln("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      connectToMqtt();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      debugPrintln("WiFi lost connection");
      break;
    default:
      break;
  }
}

void mqttTask(void *pv) {
  while(1) {
    // Process message queue
    if (mqttClient.connected() && !messageQueue.empty()) {
      String message = messageQueue.front();
      uint16_t packetId = mqttClient.publish("lock/mqtt", 1, true, message.c_str());
      debugPrintln("Publishing (QoS 1): " + message + " PID: " + String(packetId));
      messageQueue.pop();
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

bool connectToServer(BLEAddress pAddress) {
  BLEClient* pClient = BLEDevice::createClient();
  pClient->setMTU(517);
  pClient->setClientCallbacks(new MyClientCallback());

  if (!pClient->connect(pAddress)) {
    debugPrintln("BLE Connection failed");
    return false;
  }

  BLERemoteService* pRemoteService = pClient->getService(bleServiceUUID);
  if (pRemoteService == nullptr) {
    debugPrintln("Service not found");
    return false;
  }

  gyroCharacteristic = pRemoteService->getCharacteristic(gyroCharacteristicUUID);
  if (gyroCharacteristic == nullptr) {
    debugPrintln("Characteristic missing");
    return false;
  }

  gyroCharacteristic->registerForNotify(gyroNotifyCallback);
  Connected = true;
  debugPrintln("Connected to BLE");
  return true;
}

void bleTask(void *pv) {
  while(1) {
    if (doConnect) {
      if (connectToServer(*pServerAddress)) {
        doConnect = false;
      } else {
        debugPrintln("Connect failed - Scanning");
        pBLEScan->start(5);
      }
    }
    
    if (newGyro) {
      messageQueue.push(String(gyroBuffer));
      debugPrintln("Queued: " + String(gyroBuffer));
      newGyro = false;
    }
    
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void setup() {
  M5.begin();
  Serial.begin(115200);
  
  M5.Lcd.setRotation(3);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.fillScreen(BLACK);
  debugPrintln("Initializing...");

  // Memory optimization
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  // Setup WiFi events
  WiFi.onEvent(WiFiEvent);

  // Connect to WiFi
  WiFi.begin(ssid, password);

  // Configure MQTT client
  mqttClient.setServer(mqttBroker, mqttPort);
  mqttClient.setClientId("M5StickCPlus");
  mqttClient.setKeepAlive(10);
  mqttClient.setCleanSession(false); // For QoS 1 persistent sessions
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onPublish(onMqttPublish);

  // Setup BLE (unchanged)
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();

  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5);

  // Create tasks
  xTaskCreatePinnedToCore(
    mqttTask,
    "MQTT_Task",
    4096,
    NULL,
    1,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    bleTask,
    "BLE_Task",
    4096,
    NULL,
    2,
    NULL,
    0
  );
}

void loop() {
  delay(1000);
}
