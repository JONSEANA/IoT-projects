#include <WiFi.h>
#include <PubSubClient.h>
#include "BLEDevice.h"
#include <M5StickCPlus.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Wi-Fi/MQTT Config
const char* ssid = "AndroidAP284C";
const char* password = "cufj4615";
const char* mqttBroker = "192.168.59.37";
const int mqttPort = 1883;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// BLE Config
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

// BLE Notification Callback
static void gyroNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                               uint8_t* pData, size_t length, bool isNotify) {
  if (length < sizeof(gyroBuffer)) {
    memset(gyroBuffer, 0, sizeof(gyroBuffer));
    strncpy(gyroBuffer, (char*)pData, length);
    newGyro = true;
    debugPrintln("BLE Received: " + String(gyroBuffer));
  }
}

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

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == bleServerName)) {
      advertisedDevice.getScan()->stop();
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;
      debugPrintln("Device found! Connecting...");
    }
  }
};

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

void mqttTask(void *pv) {
  while(1) {
    if (!mqttClient.connected()) {
      debugPrintln("MQTT Disconnected");
      while (!mqttClient.connect("M5StickCPlus", "jiamin", "jiamin")) {
        debugPrintln("MQTT Retrying...");
        delay(5000);
      }
      debugPrintln("MQTT Connected");
    }
    mqttClient.loop();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
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
      mqttClient.publish("lock/mqtt", gyroBuffer);
      debugPrintln("Published: " + String(gyroBuffer));
      newGyro = false;
    }
    
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void setup() {
  M5.begin();
  Serial.begin(115200);
  
  // Initialize display
  M5.Lcd.setRotation(3);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.fillScreen(BLACK);
  debugPrintln("Initializing...");

  // Memory optimization for BLE
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    debugPrintln("Connecting WiFi...");
  }
  debugPrintln("WiFi connected");
  
  // Setup MQTT
  mqttClient.setServer(mqttBroker, mqttPort);
  mqttClient.setBufferSize(512);

  // Setup BLE
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
  // FreeRTOS tasks handle everything
  delay(1000);
}