#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <math.h>
#include <string.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#if __has_include("ota_secrets.h")
#include "ota_secrets.h"
#endif

#ifndef OTA_GITHUB_TOKEN
#define OTA_GITHUB_TOKEN ""
#endif

#ifndef OTA_GITHUB_TOKEN_PART1
#define OTA_GITHUB_TOKEN_PART1 ""
#endif

#ifndef OTA_GITHUB_TOKEN_PART2
#define OTA_GITHUB_TOKEN_PART2 ""
#endif

#ifndef OTA_GITHUB_OWNER
#define OTA_GITHUB_OWNER "NamaMobility"
#endif

#ifndef OTA_GITHUB_REPO
#define OTA_GITHUB_REPO "Battery-Control-Software"
#endif

#ifndef OTA_FW_ASSET_NAME
#define OTA_FW_ASSET_NAME "firmware.bin"
#endif

#ifndef OTA_SPIFFS_ASSET_NAME
#define OTA_SPIFFS_ASSET_NAME "spiffs.bin"
#endif

#ifndef OTA_MANIFEST_PATH
#define OTA_MANIFEST_PATH "ota/manifest.json"
#endif

// Pin Definitions (Spec)
constexpr uint8_t BUTTON_PIN = 19;          // D19: Ledli buton (input)
constexpr uint8_t EMERGENCY_STOP_PIN = 18;  // D18: Acil stop (input)
constexpr uint8_t CONTACT_SWITCH_PIN = 17;  // TX2: Kontak anahtari (GND aktif)
constexpr uint8_t BMS_SIGNAL_PIN = 4;       // D4: BMS sinyali (HIGH aktif)

constexpr uint8_t LED_PIN = 23;             // D23: Ledli buton LED (output)
constexpr uint8_t PRECHARGE_RELAY_PIN = 13; // D13: Presarj rolesi (HIGH aktif)
constexpr uint8_t CHARGE_RELAY_PIN = 14;    // D14: Sarj rolesi (HIGH aktif)
constexpr uint8_t DISCHARGE_RELAY_PIN = 27; // D27: Desarj rolesi (HIGH aktif)
constexpr uint8_t MOSFET1_PIN = 33;         // D33: MOSFET1 (LOW aktif)
constexpr uint8_t MOSFET2_PIN = 32;         // D32: MOSFET2 (LOW aktif)

constexpr uint8_t CURRENT_SENSOR1_PIN = 33; // Hass400S sensor 1 (ADC)
constexpr uint8_t CURRENT_SENSOR2_PIN = 32; // Hass400S sensor 2 (ADC)

constexpr uint32_t EEPROM_MAGIC = 0xB052026;
constexpr uint16_t EEPROM_VERSION = 5;
constexpr size_t EEPROM_SIZE = 1024;

constexpr float ADC_REF_VOLT = 3.3f;
constexpr float ADC_MAX = 4095.0f;
constexpr float HASS_ZERO_VOLT = 1.65f;
constexpr float HASS_SENSITIVITY = 0.025f;
constexpr char CRM_FIXED_URL[] = "";
constexpr char CRM_FIXED_API_KEY[] = "nama-bms-2523f8bb-56b2-4595-9365-89825d91c97d";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

enum SystemState {
  IDLE,
  STARTUP,
  PRECHARGE,
  MOSFET_ACTIVE,
  DISCHARGE_ACTIVE,
  CONTACT_HOLD,
  RUNNING,
  BYPASS_MODE,
  FAULT,
  EMERGENCY_STOP
};

enum LedMode {
  LED_OFF,
  LED_NORMAL,
  LED_EMERGENCY,
  LED_CONTACT_CLOSED,
  LED_BYPASS
};

struct Config {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  uint8_t batteryType;
  uint16_t seriesCount;
  float cellCapacity;
  float maxChargeCurrent;
  float maxDischargeCurrent;
  float prechargeTime;
  float mosfetOnTime;
  float shortCircuitThreshold;
  float sensor1Calibration;
  float sensor2Calibration;
  float soc;
  bool bypassEnabled;
  float totalEnergyIn;
  float totalEnergyOut;
  uint32_t cycleCount;
  char wifiSsid[33];
  char wifiPass[65];
  char crmUrl[128];
  char crmApiKey[64];
};

struct EnergyRecord {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  float energyIn;
  float energyOut;
};

constexpr size_t MAX_HISTORY_DAYS = 30;
EnergyRecord energyHistory[MAX_HISTORY_DAYS];
size_t energyHistoryCount = 0;
uint32_t lastDayIndex = 0;
float lastDayEnergyIn = 0.0f;
float lastDayEnergyOut = 0.0f;
Preferences energyPrefs;
bool energyPrefsReady = false;

Config config;
AsyncWebServer server(80);

SystemState currentState = IDLE;
LedMode currentLedMode = LED_OFF;

bool emergencyStopLatched = false;
bool emergencyInputActive = false;
bool contactSwitchActive = false;
bool lastContactSwitchActive = false;
bool bmsSignalActive = false;

bool chargeRelayState = false;
bool dischargeRelayState = false;
bool prechargeRelayState = false;
bool mosfet1State = false;
bool mosfet2State = false;

float chargeCurrent = 0.0f;
float dischargeCurrent = 0.0f;
float batteryVoltage = 0.0f;
float batteryTemperature = 25.0f;

unsigned long stateStartMs = 0;
unsigned long lastSensorMs = 0;
unsigned long lastLedMs = 0;
unsigned long lastSocSaveMs = 0;
float lastSavedSoc = 100.0f;

bool buttonPressed = false;
unsigned long buttonPressStart = 0;
unsigned long lastButtonRelease = 0;
uint8_t buttonPressCount = 0;
bool shutdownPending = false;
bool ledOutputState = false;
unsigned long emergencyInputReleasedMs = 0;
bool emergencyRawInputActive = false;
unsigned long emergencyRawChangedMs = 0;
bool emergencyCommandLatched = false;
unsigned long emergencyCommandStartMs = 0;

constexpr unsigned long CRM_PUSH_INTERVAL_MS = 500;
constexpr unsigned long CRM_POLL_INTERVAL_MS = 10;
constexpr unsigned long OTA_CHECK_INTERVAL_MS = 60000;
constexpr unsigned long EMERGENCY_INPUT_DEBOUNCE_MS = 80;
constexpr unsigned long EMERGENCY_CLEAR_STABLE_MS = 800;
constexpr unsigned long UI_EMERGENCY_TIMEOUT_MS = 10000;
constexpr char FW_VERSION[] = "1.0.17";

unsigned long lastWifiCheckMs = 0;
constexpr unsigned long WIFI_CHECK_INTERVAL_MS = 30000;
bool wifiApFallbackActive = false;

volatile bool crmStartPending = false;
volatile bool crmEmergencyPending = false;
volatile bool otaCheckPending = false;
TaskHandle_t crmTaskHandle = NULL;

constexpr size_t CRM_BUTTON_QUEUE_SIZE = 16;
volatile unsigned long crmButtonQueue[CRM_BUTTON_QUEUE_SIZE];
volatile uint8_t crmButtonQueueHead = 0;
volatile uint8_t crmButtonQueueTail = 0;

void enqueueCrmButton(unsigned long durationMs) {
  uint8_t nextTail = (crmButtonQueueTail + 1) % CRM_BUTTON_QUEUE_SIZE;
  if (nextTail != crmButtonQueueHead) {
    crmButtonQueue[crmButtonQueueTail] = durationMs;
    crmButtonQueueTail = nextTail;
  }
}

const char *servicePassword = "nama2026";

void initPins();
void initWiFi();
void startAccessPoint();
bool connectToStoredWifi();
void manageWifiConnection();
bool isStoredWifiAvailable();
void prepareStaDhcp();
void initWebServer();
void loadConfig();
void saveConfig();
void applyOutputs();
void setAllOutputsOff();
void updateInputs();
void handleContactSwitchClose();
void updateSensors();
void updateEnergy();
void updateStateMachine();
void updateLed();
void startSequence();
void triggerEmergencyStop(bool fromCommand = false);
void triggerFault(const String &message);
void clearFault();
void autoClearEmergencyIfReleased();
void handleButton();
void processButtonRelease(unsigned long pressDuration, unsigned long now);
void triggerVirtualButtonPress(unsigned long pressDurationMs);
float readCurrentSensor(uint8_t pin, float calibration);
void updateDerivedConfig();
void addEnergyHistory(float energyIn, float energyOut);
void clearEnergyHistory();
void initEnergyStorage();
void loadEnergyHistoryFromStorage();
void saveEnergyHistoryToStorage();
void initDisplay();
void updateDisplay();
bool applyFixedCrmConfig();
String buildStatusJsonString();
void pushTelemetryToCrm();
void pollCommandsFromCrm();
void processCrmCommands();
void crmTask(void *param);
String normalizeVersionTag(const String &versionTag);
int compareVersionStrings(const String &a, const String &b);
String getOtaGithubToken();
bool downloadAndApplyOtaAsset(const String &assetApiUrl, int updateCommand, const String &assetLabel, bool restartAfter);
void checkForOtaUpdate();

String faultMessage;

void setup() {
  Serial.begin(115200);

  EEPROM.begin(EEPROM_SIZE);
  loadConfig();
  initEnergyStorage();
  loadEnergyHistoryFromStorage();

  initPins();
  emergencyRawInputActive = digitalRead(EMERGENCY_STOP_PIN) == LOW;
  emergencyInputActive = emergencyRawInputActive;
  emergencyRawChangedMs = millis();
  initWiFi();

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS init failed");
  }

  initWebServer();
  initDisplay();

  setAllOutputsOff();
  currentState = IDLE;
  
  if (energyHistoryCount == 0) {
    lastDayIndex = millis() / 86400000UL;
    lastDayEnergyIn = config.totalEnergyIn;
    lastDayEnergyOut = config.totalEnergyOut;
    uint16_t year = 2026;
    uint8_t month = 2;
    uint8_t day = 11;
    energyHistory[0] = {year, month, day, 0.0f, 0.0f};
    energyHistoryCount = 1;
    saveEnergyHistoryToStorage();
  }
  
  Serial.println("BMS System Initialized");

  xTaskCreatePinnedToCore(crmTask, "CRM", 8192, NULL, 1, &crmTaskHandle, 0);
}

void loop() {
  updateInputs();
  autoClearEmergencyIfReleased();
  handleButton();
  updateSensors();
  updateStateMachine();
  updateLed();
  updateDisplay();
  processCrmCommands();

  manageWifiConnection();
}

void initPins() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(EMERGENCY_STOP_PIN, INPUT_PULLUP);
  pinMode(CONTACT_SWITCH_PIN, INPUT_PULLUP);
  pinMode(BMS_SIGNAL_PIN, INPUT_PULLUP);

  pinMode(LED_PIN, OUTPUT);
  pinMode(PRECHARGE_RELAY_PIN, OUTPUT);
  pinMode(CHARGE_RELAY_PIN, OUTPUT);
  pinMode(DISCHARGE_RELAY_PIN, OUTPUT);
  pinMode(MOSFET1_PIN, OUTPUT);
  pinMode(MOSFET2_PIN, OUTPUT);

  setAllOutputsOff();
}

void initWiFi() {
  if (strlen(config.wifiSsid) > 0) {
    if (connectToStoredWifi()) {
      return;
    }
  }
  startAccessPoint();
}

bool connectToStoredWifi() {
  WiFi.mode(WIFI_STA);
  prepareStaDhcp();
  WiFi.begin(config.wifiSsid, config.wifiPass);
  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 15000UL) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE) {
    Serial.println("WiFi STA Connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DHCP DNS: ");
    Serial.println(WiFi.dnsIP());

    // DNS testi
    IPAddress testIp;
    if (WiFi.hostByName("namacloud.com", testIp)) {
      Serial.print("DNS test OK: namacloud.com -> ");
      Serial.println(testIp);
    } else {
      Serial.println("DNS test FAILED: namacloud.com could not be resolved!");
    }

    return true;
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return false;
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP_STA);
  IPAddress localIp(192, 168, 25, 25);
  IPAddress gateway(192, 168, 25, 25);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(localIp, gateway, subnet);
  WiFi.softAP("Nama-BMS", "12345678");
  wifiApFallbackActive = true;

  Serial.println("WiFi AP Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
}

void prepareStaDhcp() {
  WiFi.disconnect(true, true);
  delay(120);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
}

bool isStoredWifiAvailable() {
  int count = WiFi.scanNetworks(false, true, false, 120);
  if (count <= 0) {
    WiFi.scanDelete();
    return false;
  }

  bool found = false;
  String target = String(config.wifiSsid);
  for (int i = 0; i < count; i++) {
    if (WiFi.SSID(i) == target) {
      found = true;
      break;
    }
  }
  WiFi.scanDelete();
  return found;
}

void manageWifiConnection() {
  if (strlen(config.wifiSsid) == 0) {
    if (!wifiApFallbackActive) {
      startAccessPoint();
    }
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (wifiApFallbackActive) {
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      wifiApFallbackActive = false;
      Serial.printf("WiFi reconnected, AP fallback disabled. IP: %s\n", WiFi.localIP().toString().c_str());
    }
    return;
  }

  if (!wifiApFallbackActive) {
    Serial.println("WiFi disconnected, switching to AP fallback...");
    startAccessPoint();
  }

  unsigned long now = millis();
  if (now - lastWifiCheckMs < WIFI_CHECK_INTERVAL_MS) return;
  lastWifiCheckMs = now;

  Serial.printf("Scanning for stored WiFi: %s\n", config.wifiSsid);
  if (!isStoredWifiAvailable()) {
    Serial.println("Stored WiFi not found. Staying in AP fallback mode.");
    return;
  }

  Serial.println("Stored WiFi found. Trying reconnect...");
  prepareStaDhcp();
  WiFi.begin(config.wifiSsid, config.wifiPass);

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 8000UL) {
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    wifiApFallbackActive = false;
    Serial.printf("WiFi reconnected from AP fallback. IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("Reconnect failed. Continuing AP fallback mode.");
  }
}

void initWebServer() {
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<1024> doc;
    doc["soc"] = config.soc;
    doc["voltage"] = batteryVoltage;
    doc["temperature"] = batteryTemperature;
    doc["capacity"] = (config.seriesCount * config.cellCapacity * (config.batteryType == 0 ? 3.2f : (config.batteryType == 1 ? 3.6f : 3.7f))) / 1000.0f;
    doc["cycleCount"] = config.cycleCount;
    doc["chargeCurrent"] = chargeCurrent;
    doc["chargePower"] = (chargeCurrent * batteryVoltage) / 1000.0f;
    doc["dischargeCurrent"] = dischargeCurrent;
    doc["dischargePower"] = (dischargeCurrent * batteryVoltage) / 1000.0f;
    doc["totalEnergyIn"] = config.totalEnergyIn;
    doc["totalEnergyOut"] = config.totalEnergyOut;
    doc["chargeRelay"] = chargeRelayState;
    doc["dischargeRelay"] = dischargeRelayState;
    doc["prechargeRelay"] = prechargeRelayState;
    doc["mosfet1"] = mosfet1State;
    doc["mosfet2"] = mosfet2State;
    doc["bmsSignal"] = bmsSignalActive || config.bypassEnabled;
    doc["contactSwitch"] = contactSwitchActive;
    doc["bypassMode"] = config.bypassEnabled;
    doc["emergencyStop"] = emergencyStopLatched;

    const char *stateText = "IDLE";
    switch (currentState) {
      case IDLE: stateText = "IDLE"; break;
      case STARTUP: stateText = "STARTUP"; break;
      case PRECHARGE: stateText = "PRECHARGE"; break;
      case MOSFET_ACTIVE: stateText = "MOSFET_ACTIVE"; break;
      case DISCHARGE_ACTIVE: stateText = "DISCHARGE_ACTIVE"; break;
      case CONTACT_HOLD: stateText = "CONTACT_HOLD"; break;
      case RUNNING: stateText = "RUNNING"; break;
      case BYPASS_MODE: stateText = "BYPASS_MODE"; break;
      case FAULT: stateText = "FAULT"; break;
      case EMERGENCY_STOP: stateText = "EMERGENCY_STOP"; break;
    }
    doc["systemState"] = stateText;

    const char *ledText = "OFF";
    switch (currentLedMode) {
      case LED_NORMAL: ledText = "NORMAL"; break;
      case LED_EMERGENCY: ledText = "EMERGENCY"; break;
      case LED_CONTACT_CLOSED: ledText = "CONTACT_CLOSED"; break;
      case LED_BYPASS: ledText = "BYPASS"; break;
      case LED_OFF: default: ledText = "OFF"; break;
    }
    doc["ledMode"] = ledText;
    doc["ledOn"] = ledOutputState;
    doc["faultMessage"] = faultMessage;
    doc["mac"] = WiFi.softAPmacAddress();

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
  });

  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<768> doc;
    doc["batteryType"] = config.batteryType;
    doc["seriesCount"] = config.seriesCount;
    doc["cellCapacity"] = config.cellCapacity;
    doc["maxChargeCurrent"] = config.maxChargeCurrent;
    doc["maxDischargeCurrent"] = config.maxDischargeCurrent;
    doc["prechargeTime"] = config.prechargeTime;
    doc["mosfetOnTime"] = config.mosfetOnTime;
    doc["shortCircuitThreshold"] = config.shortCircuitThreshold;
    doc["sensor1Calibration"] = config.sensor1Calibration;
    doc["sensor2Calibration"] = config.sensor2Calibration;
    doc["wifiSsid"] = config.wifiSsid;
    doc["wifiHasPass"] = config.wifiPass[0] != '\0';

    updateDerivedConfig();
    doc["batteryCapacity"] = (config.seriesCount * config.cellCapacity * (config.batteryType == 0 ? 3.2f : (config.batteryType == 1 ? 3.6f : 3.7f))) / 1000.0f;
    doc["maxVoltage"] = (config.batteryType == 0 ? 3.65f : 4.2f) * config.seriesCount;
    doc["minVoltage"] = (config.batteryType == 0 ? 2.5f : 3.0f) * config.seriesCount;

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
  });

  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      if (!request->hasHeader("X-Service-Password") ||
          request->getHeader("X-Service-Password")->value() != servicePassword) {
        request->send(403, "application/json", "{\"success\":false,\"error\":\"forbidden\"}");
        return;
      }

      StaticJsonDocument<512> doc;
      if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
        request->send(400, "application/json", "{\"success\":false}");
        return;
      }

      config.batteryType = doc["batteryType"] | config.batteryType;
      config.seriesCount = doc["seriesCount"] | config.seriesCount;
      config.cellCapacity = doc["cellCapacity"] | config.cellCapacity;
      config.maxChargeCurrent = doc["maxChargeCurrent"] | config.maxChargeCurrent;
      config.maxDischargeCurrent = doc["maxDischargeCurrent"] | config.maxDischargeCurrent;
      config.prechargeTime = doc["prechargeTime"] | config.prechargeTime;
      config.mosfetOnTime = doc["mosfetOnTime"] | config.mosfetOnTime;
      config.shortCircuitThreshold = doc["shortCircuitThreshold"] | config.shortCircuitThreshold;
      config.sensor1Calibration = doc["sensor1Calibration"] | config.sensor1Calibration;
      config.sensor2Calibration = doc["sensor2Calibration"] | config.sensor2Calibration;

      bool wifiUpdated = false;
      if (doc.containsKey("wifiSsid")) {
        String ssid = doc["wifiSsid"].as<String>();
        ssid.trim();
        ssid.toCharArray(config.wifiSsid, sizeof(config.wifiSsid));
        wifiUpdated = true;
      }
      if (doc.containsKey("wifiPass")) {
        String pass = doc["wifiPass"].as<String>();
        pass.toCharArray(config.wifiPass, sizeof(config.wifiPass));
        wifiUpdated = true;
      }

      applyFixedCrmConfig();

      saveConfig();
      request->send(200, "application/json", "{\"success\":true}");

      if (wifiUpdated) {
        initWiFi();
      }
    });

  server.on("/api/scan-networks", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<2048> doc;
    JsonArray networks = doc.createNestedArray("networks");
    
    // Save current WiFi mode
    wifi_mode_t currentMode = WiFi.getMode();
    
    // Switch to STA mode for scanning (required for WiFi.scanNetworks() to work)
    if (currentMode != WIFI_STA && currentMode != WIFI_AP_STA) {
      WiFi.mode(WIFI_STA);
      delay(100); // Give WiFi radio time to switch modes
    }
    
    // Perform WiFi scan with increased timeout
    int n = WiFi.scanNetworks(false, false, false, 200);
    
    // Collect networks from scan results
    for (int i = 0; i < n; i++) {
      JsonObject net = networks.createNestedObject();
      net["ssid"] = WiFi.SSID(i);
      net["rssi"] = WiFi.RSSI(i);
      net["channel"] = WiFi.channel(i);
      net["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    
    // Restore previous WiFi mode
    if (currentMode == WIFI_AP) {
      WiFi.mode(WIFI_AP);
      delay(100);
    }
    
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
  });

  server.on("/api/sequence", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      deserializeJson(doc, data, len);
      String type = doc["type"] | "";
      if (type == "start") {
        startSequence();
      }
      request->send(200, "application/json", "{\"success\":true}");
    });

  server.on("/api/emergency", HTTP_POST, [](AsyncWebServerRequest *request){
    triggerEmergencyStop(true);
    request->send(200, "application/json", "{\"success\":true}");
  });

  server.on("/api/control", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      deserializeJson(doc, data, len);
      String relay = doc["relay"] | "";
      String action = doc["action"] | "";

      if (action == "toggle" && !emergencyStopLatched && currentState == IDLE) {
        if (relay == "charge") chargeRelayState = !chargeRelayState;
        if (relay == "discharge") dischargeRelayState = !dischargeRelayState;
        if (relay == "precharge") prechargeRelayState = !prechargeRelayState;
        if (relay == "mosfet1") mosfet1State = !mosfet1State;
        if (relay == "mosfet2") mosfet2State = !mosfet2State;
        applyOutputs();
      }

      request->send(200, "application/json", "{\"success\":true}");
    });

  server.on("/api/calibrate", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
        request->send(400, "application/json", "{\"success\":false}");
        return;
      }
      int sensor = doc["sensor"] | 0;
      float referenceCurrent = doc["referenceCurrent"] | 0.0f;
      float raw = 0.0f;

      if (sensor == 1) raw = readCurrentSensor(CURRENT_SENSOR1_PIN, 1.0f);
      if (sensor == 2) raw = readCurrentSensor(CURRENT_SENSOR2_PIN, 1.0f);

      if (raw == 0.0f || referenceCurrent == 0.0f) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"invalid\"}");
        return;
      }

      float factor = referenceCurrent / raw;
      if (sensor == 1) config.sensor1Calibration = factor;
      if (sensor == 2) config.sensor2Calibration = factor;
      saveConfig();

      StaticJsonDocument<256> res;
      res["calibrationFactor"] = factor;
      AsyncResponseStream *response = request->beginResponseStream("application/json");
      serializeJson(res, *response);
      request->send(response);
    });

  server.on("/api/energy-history", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<4096> doc;
    JsonArray records = doc["records"].to<JsonArray>();
    for (size_t i = 0; i < energyHistoryCount; ++i) {
      JsonObject item = records.add<JsonObject>();
      item["year"] = energyHistory[i].year;
      item["month"] = energyHistory[i].month;
      item["day"] = energyHistory[i].day;
      item["energyIn"] = energyHistory[i].energyIn;
      item["energyOut"] = energyHistory[i].energyOut;
    }
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
  });

  server.on("/api/clear-history", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!request->hasHeader("X-Service-Password") ||
        request->getHeader("X-Service-Password")->value() != servicePassword) {
      request->send(403, "application/json", "{\"success\":false,\"error\":\"forbidden\"}");
      return;
    }
    clearEnergyHistory();
    request->send(200, "application/json", "{\"success\":true}");
  });

  server.on("/api/button", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      StaticJsonDocument<128> doc;
      unsigned long pressDurationMs = 120;

      if (len > 0 && deserializeJson(doc, data, len) == DeserializationError::Ok) {
        pressDurationMs = doc["durationMs"] | pressDurationMs;
      }

      if (pressDurationMs < 50) pressDurationMs = 50;
      if (pressDurationMs > 20000) pressDurationMs = 20000;

      triggerVirtualButtonPress(pressDurationMs);
      request->send(200, "application/json", "{\"success\":true}");
    });

  server.on("/api/ota-check", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!request->hasHeader("X-Service-Password") ||
        request->getHeader("X-Service-Password")->value() != servicePassword) {
      request->send(403, "application/json", "{\"success\":false,\"error\":\"forbidden\"}");
      return;
    }

    if (WiFi.status() != WL_CONNECTED) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"wifi_not_connected\"}");
      return;
    }

    otaCheckPending = true;
    request->send(202, "application/json", "{\"success\":true,\"queued\":true}");
  });

  server.begin();
  Serial.println("Web Server Started");
}

void loadConfig() {
  bool needsSave = false;

  EEPROM.get(0, config);
  if (config.magic != EEPROM_MAGIC) {
    config.magic = EEPROM_MAGIC;
    config.version = EEPROM_VERSION;
    config.batteryType = 0;
    config.seriesCount = 24;
    config.cellCapacity = 314.0f;
    config.maxChargeCurrent = 300.0f;
    config.maxDischargeCurrent = 600.0f;
    config.prechargeTime = 10.0f;
    config.mosfetOnTime = 0.0f;
    config.shortCircuitThreshold = 800.0f;
    config.sensor1Calibration = 1.0f;
    config.sensor2Calibration = 1.0f;
    config.soc = 100.0f;
    config.bypassEnabled = false;
    config.totalEnergyIn = 0.0f;
    config.totalEnergyOut = 0.0f;
    config.cycleCount = 0;
    config.wifiSsid[0] = '\0';
    config.wifiPass[0] = '\0';
    config.crmUrl[0] = '\0';
    config.crmApiKey[0] = '\0';
    needsSave = true;
  }
  else if (config.version != EEPROM_VERSION) {
    if (config.version == 3) {
      config.version = EEPROM_VERSION;
      config.crmUrl[0] = '\0';
      config.crmApiKey[0] = '\0';
      needsSave = true;
    } else if (config.version == 4) {
      config.version = EEPROM_VERSION;
      config.seriesCount = 24;
      config.cellCapacity = 314.0f;
      config.maxChargeCurrent = 300.0f;
      config.maxDischargeCurrent = 600.0f;
      config.prechargeTime = 10.0f;
      config.mosfetOnTime = 0.0f;
      config.shortCircuitThreshold = 800.0f;
      needsSave = true;
    } else {
      config.version = EEPROM_VERSION;
      config.batteryType = 0;
      config.seriesCount = 24;
      config.cellCapacity = 314.0f;
      config.maxChargeCurrent = 300.0f;
      config.maxDischargeCurrent = 600.0f;
      config.prechargeTime = 10.0f;
      config.mosfetOnTime = 0.0f;
      config.shortCircuitThreshold = 800.0f;
      config.sensor1Calibration = 1.0f;
      config.sensor2Calibration = 1.0f;
      config.soc = 100.0f;
      config.bypassEnabled = false;
      config.totalEnergyIn = 0.0f;
      config.totalEnergyOut = 0.0f;
      config.cycleCount = 0;
      config.wifiSsid[0] = '\0';
      config.wifiPass[0] = '\0';
      config.crmUrl[0] = '\0';
      config.crmApiKey[0] = '\0';
      needsSave = true;
    }
  }

  if (applyFixedCrmConfig() || needsSave) {
    saveConfig();
  }
}

void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
}

bool applyFixedCrmConfig() {
  bool changed = false;

  if (strcmp(config.crmUrl, CRM_FIXED_URL) != 0) {
    strncpy(config.crmUrl, CRM_FIXED_URL, sizeof(config.crmUrl) - 1);
    config.crmUrl[sizeof(config.crmUrl) - 1] = '\0';
    changed = true;
  }

  if (strcmp(config.crmApiKey, CRM_FIXED_API_KEY) != 0) {
    strncpy(config.crmApiKey, CRM_FIXED_API_KEY, sizeof(config.crmApiKey) - 1);
    config.crmApiKey[sizeof(config.crmApiKey) - 1] = '\0';
    changed = true;
  }

  return changed;
}

void updateInputs() {
  lastContactSwitchActive = contactSwitchActive;
  // CONTACT_SWITCH_PIN: GND aktif
  contactSwitchActive = digitalRead(CONTACT_SWITCH_PIN) == LOW;
  // BMS_SIGNAL_PIN: HIGH aktif
  bmsSignalActive = digitalRead(BMS_SIGNAL_PIN) == HIGH;

  bool rawEmergencyActive = digitalRead(EMERGENCY_STOP_PIN) == LOW;
  unsigned long now = millis();

  if (rawEmergencyActive != emergencyRawInputActive) {
    emergencyRawInputActive = rawEmergencyActive;
    emergencyRawChangedMs = now;
  }

  if (now - emergencyRawChangedMs >= EMERGENCY_INPUT_DEBOUNCE_MS) {
    emergencyInputActive = emergencyRawInputActive;
  }

  if (emergencyInputActive) {
    triggerEmergencyStop(false);
    emergencyInputReleasedMs = 0;
  } else if (emergencyInputReleasedMs == 0) {
    // Latch is cleared only after emergency input remains released for a short stable time.
    emergencyInputReleasedMs = now;
  }

  if (contactSwitchActive && !lastContactSwitchActive) {
    handleContactSwitchClose();
  }
}

void autoClearEmergencyIfReleased() {
  if (!emergencyStopLatched) return;

  unsigned long now = millis();

  if (emergencyCommandLatched && now - emergencyCommandStartMs >= UI_EMERGENCY_TIMEOUT_MS) {
    emergencyCommandLatched = false;
    emergencyCommandStartMs = 0;
    Serial.println("UI emergency timeout elapsed");
  }

  if (emergencyInputActive || emergencyCommandLatched) {
    emergencyInputReleasedMs = 0;
    return;
  }

  if (emergencyInputReleasedMs == 0) {
    emergencyInputReleasedMs = now;
    return;
  }

  if (now - emergencyInputReleasedMs < EMERGENCY_CLEAR_STABLE_MS) return;

  emergencyStopLatched = false;
  faultMessage = "";
  currentState = IDLE;
  emergencyCommandStartMs = 0;
  setAllOutputsOff();
  Serial.println("Emergency stop auto-cleared (input released)");
}

void updateSensors() {
  unsigned long now = millis();
  if (now - lastSensorMs < 100) return;
  float dt = (now - lastSensorMs) / 1000.0f;
  lastSensorMs = now;

  chargeCurrent = readCurrentSensor(CURRENT_SENSOR1_PIN, config.sensor1Calibration);
  dischargeCurrent = readCurrentSensor(CURRENT_SENSOR2_PIN, config.sensor2Calibration);

  float maxVoltage = (config.batteryType == 0 ? 3.65f : 4.2f) * config.seriesCount;
  float minVoltage = (config.batteryType == 0 ? 2.5f : 3.0f) * config.seriesCount;
  batteryVoltage = minVoltage + (maxVoltage - minVoltage) * (config.soc / 100.0f);

  updateEnergy();

  if (emergencyStopLatched) return;

  if (!config.bypassEnabled && !bmsSignalActive && currentState != IDLE && currentState != FAULT && currentState != EMERGENCY_STOP) {
    triggerFault("BMS sinyali kaybi");
  }

  if (fabs(chargeCurrent) > config.shortCircuitThreshold || fabs(dischargeCurrent) > config.shortCircuitThreshold) {
    triggerFault("Kisa devre akimi");
  }

  if (fabs(chargeCurrent) > config.maxChargeCurrent) {
    triggerFault("Maksimum sarj akimi asildi");
  }

  if (fabs(dischargeCurrent) > config.maxDischargeCurrent) {
    triggerFault("Maksimum desarj akimi asildi");
  }
}

void updateEnergy() {
  static unsigned long lastEnergyMs = 0;
  unsigned long now = millis();
  if (lastEnergyMs == 0) {
    lastEnergyMs = now;
    lastSocSaveMs = now;
    lastSavedSoc = config.soc;
    return;
  }
  float dt = (now - lastEnergyMs) / 1000.0f;
  lastEnergyMs = now;

  float netCurrent = chargeCurrent - dischargeCurrent;
  float capacityAh = config.cellCapacity;
  if (capacityAh <= 0.1f) return;

  float deltaAh = netCurrent * (dt / 3600.0f);
  config.soc += (deltaAh / capacityAh) * 100.0f;
  if (config.soc > 100.0f) config.soc = 100.0f;
  if (config.soc < 0.0f) config.soc = 0.0f;

  float chargePowerKw = (chargeCurrent * batteryVoltage) / 1000.0f;
  float dischargePowerKw = (dischargeCurrent * batteryVoltage) / 1000.0f;

  if (chargePowerKw > 0.0f) config.totalEnergyIn += chargePowerKw * (dt / 3600.0f);
  if (dischargePowerKw > 0.0f) config.totalEnergyOut += dischargePowerKw * (dt / 3600.0f);

  if (config.totalEnergyOut > 0.0f && config.totalEnergyIn > 0.0f) {
    config.cycleCount = static_cast<uint32_t>((config.totalEnergyOut + config.totalEnergyIn) / (config.cellCapacity * 3.2f / 1000.0f));
  }

  addEnergyHistory(config.totalEnergyIn, config.totalEnergyOut);

  if (now - lastSocSaveMs >= 60000 || fabs(config.soc - lastSavedSoc) >= 1.0f) {
    lastSocSaveMs = now;
    lastSavedSoc = config.soc;
    saveConfig();
    saveEnergyHistoryToStorage();
  }
}

void handleContactSwitchClose() {
  if (currentState == IDLE || currentState == BYPASS_MODE) {
    return;
  }

  if (dischargeRelayState || prechargeRelayState || mosfet1State || mosfet2State) {
    chargeRelayState = true;
    dischargeRelayState = false;
    prechargeRelayState = false;
    mosfet1State = false;
    mosfet2State = false;
    applyOutputs();
  }

  currentState = CONTACT_HOLD;
}

void updateStateMachine() {
  if (emergencyStopLatched) {
    currentState = EMERGENCY_STOP;
    setAllOutputsOff();
    return;
  }

  if (currentState == FAULT) {
    setAllOutputsOff();
    return;
  }

  unsigned long now = millis();

  if (contactSwitchActive && currentState != IDLE && currentState != BYPASS_MODE && currentState != CONTACT_HOLD) {
    currentState = CONTACT_HOLD;
  }

  if (currentState == IDLE && config.bypassEnabled) {
    currentState = BYPASS_MODE;
  }

  switch (currentState) {
    case IDLE:
      setAllOutputsOff();
      break;

    case BYPASS_MODE:
      if (!config.bypassEnabled) {
        currentState = IDLE;
        break;
      }
      break;

    case STARTUP:
      chargeRelayState = true;
      prechargeRelayState = true;
      dischargeRelayState = false;
      mosfet1State = false;
      mosfet2State = false;
      applyOutputs();
      if (now - stateStartMs >= 500) {
        currentState = PRECHARGE;
        stateStartMs = now;
      }
      break;

    case PRECHARGE:
      chargeRelayState = true;
      prechargeRelayState = true;
      dischargeRelayState = false;
      mosfet1State = false;
      mosfet2State = false;
      applyOutputs();
      if (now - stateStartMs >= static_cast<unsigned long>(config.prechargeTime * 1000.0f)) {
        prechargeRelayState = false;
        mosfet1State = true;
        mosfet2State = true;
        currentState = MOSFET_ACTIVE;
        stateStartMs = now;
      }
      break;

    case MOSFET_ACTIVE: {
      chargeRelayState = true;
      prechargeRelayState = false;
      dischargeRelayState = false;
      mosfet1State = true;
      mosfet2State = true;
      applyOutputs();
      float halfDuration = config.mosfetOnTime * 1000.0f * 0.5f;
      if (now - stateStartMs >= static_cast<unsigned long>(halfDuration)) {
        dischargeRelayState = true;
        currentState = DISCHARGE_ACTIVE;
      }
      break;
    }

    case DISCHARGE_ACTIVE: {
      chargeRelayState = true;
      prechargeRelayState = false;
      dischargeRelayState = true;
      mosfet1State = true;
      mosfet2State = true;
      applyOutputs();
      if (now - stateStartMs >= static_cast<unsigned long>(config.mosfetOnTime * 1000.0f)) {
        mosfet1State = false;
        mosfet2State = false;
        currentState = RUNNING;
      }
      break;
    }

    case CONTACT_HOLD:
      chargeRelayState = true;
      dischargeRelayState = false;
      prechargeRelayState = false;
      mosfet1State = false;
      mosfet2State = false;
      applyOutputs();
      if (!contactSwitchActive) {
        if (!bmsSignalActive && !config.bypassEnabled) {
          currentState = IDLE;
          setAllOutputsOff();
          break;
        }
        currentState = STARTUP;
        stateStartMs = now;
      }
      break;

    case RUNNING:
      chargeRelayState = true;
      prechargeRelayState = false;
      dischargeRelayState = true;
      mosfet1State = false;
      mosfet2State = false;
      applyOutputs();
      break;

    case FAULT:
    case EMERGENCY_STOP:
      setAllOutputsOff();
      break;
  }
}

void updateLed() {
  unsigned long now = millis();
  static bool ledOn = false;
  static unsigned long phaseStart = 0;
  static LedMode lastMode = LED_OFF;

  if (emergencyStopLatched || currentState == FAULT) {
    currentLedMode = LED_EMERGENCY;
  } else if (config.bypassEnabled) {
    currentLedMode = LED_BYPASS;
  } else if ((currentState == IDLE || currentState == BYPASS_MODE) && contactSwitchActive) {
    currentLedMode = LED_CONTACT_CLOSED;
  } else if (currentState == IDLE) {
    currentLedMode = LED_OFF;
  } else {
    currentLedMode = LED_NORMAL;
  }

  if (currentLedMode != lastMode) {
    lastMode = currentLedMode;
    phaseStart = 0;
    ledOn = false;
  }

  if (currentLedMode == LED_BYPASS) {
    ledOn = true;
  } else if (currentLedMode == LED_OFF) {
    ledOn = false;
  } else {
    unsigned long onTime = 1000;
    unsigned long offTime = 1000;

    if (currentLedMode == LED_EMERGENCY) {
      onTime = 100;
      offTime = 100;
    } else if (currentLedMode == LED_CONTACT_CLOSED) {
      onTime = 5000;
      offTime = 150;
    }

    if (phaseStart == 0) {
      phaseStart = now;
      ledOn = true;
    } else if (ledOn && now - phaseStart >= onTime) {
      ledOn = false;
      phaseStart = now;
    } else if (!ledOn && now - phaseStart >= offTime) {
      ledOn = true;
      phaseStart = now;
    }
  }

  digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
  ledOutputState = ledOn;
}

void handleButton() {
  bool pressed = digitalRead(BUTTON_PIN) == LOW;
  unsigned long now = millis();

  if (pressed && !buttonPressed) {
    buttonPressed = true;
    buttonPressStart = now;
  }

  if (pressed && buttonPressed) {
    unsigned long pressDuration = now - buttonPressStart;
    if (pressDuration >= 15000 && (currentState == FAULT || emergencyStopLatched)) {
      clearFault();
      buttonPressed = false;
      return;
    }
  }

  if (!pressed && buttonPressed) {
    unsigned long pressDuration = now - buttonPressStart;
    processButtonRelease(pressDuration, now);
  }
}

void processButtonRelease(unsigned long pressDuration, unsigned long now) {
  unsigned long sinceLastRelease = now - lastButtonRelease;
  buttonPressed = false;
  lastButtonRelease = now;

  if (pressDuration >= 3000 && pressDuration <= 6000) {
    shutdownPending = true;
    Serial.println("Shutdown mode - press again to confirm");
    return;
  }

  if (shutdownPending) {
    currentState = IDLE;
    setAllOutputsOff();
    shutdownPending = false;
    Serial.println("System shut down");
    return;
  }

  if (sinceLastRelease > 1500) {
    buttonPressCount = 0;
  }
  buttonPressCount++;

  if (buttonPressCount >= 8) {
    if (!bmsSignalActive || config.bypassEnabled) {
      config.bypassEnabled = !config.bypassEnabled;
      saveConfig();
      buttonPressCount = 0;
      if (config.bypassEnabled) {
        currentState = BYPASS_MODE;
        Serial.println("Bypass mode enabled");
      } else {
        currentState = IDLE;
        setAllOutputsOff();
        Serial.println("Bypass mode disabled");
      }
      return;
    }
  }

  if (emergencyStopLatched && !emergencyInputActive) {
    emergencyStopLatched = false;
    faultMessage = "";
    currentState = IDLE;
    setAllOutputsOff();
    Serial.println("Emergency stop cleared");
    return;
  }

  startSequence();
}

void triggerVirtualButtonPress(unsigned long pressDurationMs) {
  if (pressDurationMs >= 15000 && (currentState == FAULT || emergencyStopLatched)) {
    clearFault();
    buttonPressed = false;
    return;
  }

  processButtonRelease(pressDurationMs, millis());
}

void startSequence() {
  if (emergencyStopLatched) return;
  if (!bmsSignalActive && !config.bypassEnabled) return;
  if (currentState != IDLE && currentState != BYPASS_MODE) return;

  if (config.bypassEnabled && currentState == BYPASS_MODE) {
    currentState = STARTUP;
    stateStartMs = millis();
    Serial.println("Starting in bypass mode");
  } else if (currentState == IDLE) {
    currentState = STARTUP;
    stateStartMs = millis();
    Serial.println("Starting sequence");
  }
}

void triggerEmergencyStop(bool fromCommand) {
  if (fromCommand) {
    emergencyCommandLatched = true;
    emergencyCommandStartMs = millis();
  }

  emergencyStopLatched = true;
  currentState = EMERGENCY_STOP;
  faultMessage = fromCommand ? "Acil durdur aktif" : "Acil stop aktif";
  setAllOutputsOff();

  if (fromCommand) {
    Serial.println("EMERGENCY STOP ACTIVATED (UI/CRM)");
  } else {
    Serial.println("EMERGENCY STOP ACTIVATED (hardware)");
  }
}

void triggerFault(const String &message) {
  if (currentState == FAULT || emergencyStopLatched) return;
  currentState = FAULT;
  faultMessage = message;
  setAllOutputsOff();
  Serial.println("FAULT STATE");
  Serial.println(message);
}

void clearFault() {
  if (emergencyStopLatched && emergencyInputActive) {
    Serial.println("Emergency input still active, clear blocked");
    return;
  }

  if (currentState == FAULT || emergencyStopLatched) {
    faultMessage = "";
    currentState = IDLE;
    emergencyStopLatched = false;
    emergencyInputReleasedMs = 0;
    emergencyCommandLatched = false;
    emergencyCommandStartMs = 0;
    setAllOutputsOff();
    Serial.println("Fault/Emergency cleared");
  }
}

void applyOutputs() {
  digitalWrite(PRECHARGE_RELAY_PIN, prechargeRelayState ? HIGH : LOW);
  digitalWrite(CHARGE_RELAY_PIN, chargeRelayState ? HIGH : LOW);
  digitalWrite(DISCHARGE_RELAY_PIN, dischargeRelayState ? HIGH : LOW);
  digitalWrite(MOSFET1_PIN, mosfet1State ? LOW : HIGH);
  digitalWrite(MOSFET2_PIN, mosfet2State ? LOW : HIGH);
}

void setAllOutputsOff() {
  chargeRelayState = false;
  dischargeRelayState = false;
  prechargeRelayState = false;
  mosfet1State = false;
  mosfet2State = false;
  applyOutputs();
}

float readCurrentSensor(uint8_t pin, float calibration) {
  int rawValue = analogRead(pin);
  float voltage = (rawValue / ADC_MAX) * ADC_REF_VOLT;
  float current = ((voltage - HASS_ZERO_VOLT) / HASS_SENSITIVITY) * calibration;
  return current;
}

void updateDerivedConfig() {
  if (config.seriesCount == 0) config.seriesCount = 1;
}

void addEnergyHistory(float energyIn, float energyOut) {
  uint32_t dayIndex = millis() / 86400000UL;
  
  if (energyHistoryCount > 0) {
    float deltaIn = energyIn - lastDayEnergyIn;
    float deltaOut = energyOut - lastDayEnergyOut;
    energyHistory[energyHistoryCount - 1].energyIn = deltaIn;
    energyHistory[energyHistoryCount - 1].energyOut = deltaOut;
  }
  
  if (dayIndex != lastDayIndex) {
    lastDayEnergyIn = energyIn;
    lastDayEnergyOut = energyOut;
    lastDayIndex = dayIndex;

    uint16_t year = 2024 + (dayIndex / (28 * 12));
    uint8_t month = (dayIndex / 28) % 12 + 1;
    uint8_t day = (dayIndex % 28) + 1;
    if (energyHistoryCount < MAX_HISTORY_DAYS) {
      energyHistory[energyHistoryCount++] = {year, month, day, 0.0f, 0.0f};
    } else {
      for (size_t i = 1; i < MAX_HISTORY_DAYS; ++i) {
        energyHistory[i - 1] = energyHistory[i];
      }
      energyHistory[MAX_HISTORY_DAYS - 1] = {year, month, day, 0.0f, 0.0f};
    }
    saveEnergyHistoryToStorage();
  }
}

void clearEnergyHistory() {
  energyHistoryCount = 0;
  lastDayIndex = 0;
  lastDayEnergyIn = 0.0f;
  lastDayEnergyOut = 0.0f;
  saveEnergyHistoryToStorage();
}

void initEnergyStorage() {
  energyPrefsReady = energyPrefs.begin("energy", false);
  if (!energyPrefsReady) {
    Serial.println("Energy storage init failed");
  }
}

void loadEnergyHistoryFromStorage() {
  if (!energyPrefsReady) return;

  uint32_t storedCount = energyPrefs.getUInt("cnt", 0);
  if (storedCount > MAX_HISTORY_DAYS) {
    storedCount = MAX_HISTORY_DAYS;
  }

  if (storedCount > 0) {
    size_t expectedSize = storedCount * sizeof(EnergyRecord);
    size_t loadedSize = energyPrefs.getBytes("rec", energyHistory, expectedSize);
    if (loadedSize == expectedSize) {
      energyHistoryCount = storedCount;
    } else {
      energyHistoryCount = 0;
    }
  } else {
    energyHistoryCount = 0;
  }

  lastDayIndex = energyPrefs.getULong("didx", millis() / 86400000UL);
  lastDayEnergyIn = energyPrefs.getFloat("din", config.totalEnergyIn);
  lastDayEnergyOut = energyPrefs.getFloat("dout", config.totalEnergyOut);
}

void saveEnergyHistoryToStorage() {
  if (!energyPrefsReady) return;

  energyPrefs.putUInt("cnt", static_cast<uint32_t>(energyHistoryCount));
  if (energyHistoryCount > 0) {
    energyPrefs.putBytes("rec", energyHistory, energyHistoryCount * sizeof(EnergyRecord));
  } else {
    energyPrefs.remove("rec");
  }
  energyPrefs.putULong("didx", lastDayIndex);
  energyPrefs.putFloat("din", lastDayEnergyIn);
  energyPrefs.putFloat("dout", lastDayEnergyOut);
}

void initDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    return;
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("BMS System");
  display.println("Initializing...");
  display.display();
  delay(500);
}

void updateDisplay() {
  static unsigned long lastDisplayMs = 0;
  unsigned long now = millis();
  if (now - lastDisplayMs < 500) return;
  lastDisplayMs = now;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  // Line 1: IP Address
  display.print("IP: ");
  if (WiFi.status() == WL_CONNECTED) {
    display.println(WiFi.localIP());
  } else {
    display.println(WiFi.softAPIP());
  }
  
  // Line 2: Status
  if (emergencyStopLatched) {
    display.println("Acil stop basildi");
  } else if (config.bypassEnabled) {
    display.println("Bypass modu aktif");
  } else if (contactSwitchActive) {
    const char *msg = "Kontak kapatildi sarj devam ediyor";
    const int windowLen = 21;
    String scroll = String(msg) + "   ";
    int totalLen = scroll.length();
    int start = (now / 250) % totalLen;
    String view = scroll.substring(start);
    if (view.length() < windowLen) {
      view += scroll.substring(0, windowLen - view.length());
    } else {
      view = view.substring(0, windowLen);
    }
    display.println(view);
  } else if (!bmsSignalActive && !config.bypassEnabled) {
    display.println("BMS sinyali kesildi");
    display.println("Kaydi silmek icin");
    display.println("15sn isikli buton");
    display.println("basin");
    display.display();
    return;
  } else if (dischargeRelayState) {
    display.println("Desarj aktif");
  } else if (prechargeRelayState) {
    display.println("Presarj aktif");
  } else if (chargeRelayState) {
    display.println("Sarj aktif");
  } else {
    display.println("Bekleme");
  }
  
  // Line 3: Battery info
  display.print("SOC:");
  display.print((int)config.soc);
  display.print("% V:");
  display.println((int)batteryVoltage);
  
  // Line 4: Current
  display.print("I:");
  display.print((int)(chargeCurrent - dischargeCurrent));
  display.print("A Con:");
  display.println(contactSwitchActive ? "ON" : "OFF");
  
  // Line 5: Relays
  display.print("Relays: C:");
  display.print(chargeRelayState ? "1" : "0");
  display.print(" D:");
  display.print(dischargeRelayState ? "1" : "0");
  display.print(" P:");
  display.println(prechargeRelayState ? "1" : "0");
  
  // Last line: Emergency stop
  if (emergencyStopLatched) {
    display.setTextSize(1);
    display.print("*** EMERGENCY STOP ***");
  }

  display.display();
}

// ---- CRM (nama_crm) entegrasyonu ----

String buildStatusJsonString() {
  DynamicJsonDocument doc(4096);
  doc["soc"] = config.soc;
  doc["voltage"] = batteryVoltage;
  doc["temperature"] = batteryTemperature;
  doc["capacity"] = (config.seriesCount * config.cellCapacity * (config.batteryType == 0 ? 3.2f : (config.batteryType == 1 ? 3.6f : 3.7f))) / 1000.0f;
  doc["cycleCount"] = config.cycleCount;
  doc["chargeCurrent"] = chargeCurrent;
  doc["chargePower"] = (chargeCurrent * batteryVoltage) / 1000.0f;
  doc["dischargeCurrent"] = dischargeCurrent;
  doc["dischargePower"] = (dischargeCurrent * batteryVoltage) / 1000.0f;
  doc["totalEnergyIn"] = config.totalEnergyIn;
  doc["totalEnergyOut"] = config.totalEnergyOut;
  doc["chargeRelay"] = chargeRelayState;
  doc["dischargeRelay"] = dischargeRelayState;
  doc["prechargeRelay"] = prechargeRelayState;
  doc["mosfet1"] = mosfet1State;
  doc["mosfet2"] = mosfet2State;
  doc["bmsSignal"] = bmsSignalActive || config.bypassEnabled;
  doc["contactSwitch"] = contactSwitchActive;
  doc["bypassMode"] = config.bypassEnabled;
  doc["emergencyStop"] = emergencyStopLatched;
  const char *stateText = "IDLE";
  switch (currentState) {
    case IDLE: stateText = "IDLE"; break;
    case STARTUP: stateText = "STARTUP"; break;
    case PRECHARGE: stateText = "PRECHARGE"; break;
    case MOSFET_ACTIVE: stateText = "MOSFET_ACTIVE"; break;
    case DISCHARGE_ACTIVE: stateText = "DISCHARGE_ACTIVE"; break;
    case CONTACT_HOLD: stateText = "CONTACT_HOLD"; break;
    case RUNNING: stateText = "RUNNING"; break;
    case BYPASS_MODE: stateText = "BYPASS_MODE"; break;
    case FAULT: stateText = "FAULT"; break;
    case EMERGENCY_STOP: stateText = "EMERGENCY_STOP"; break;
  }
  doc["systemState"] = stateText;
  const char *ledText = "OFF";
  switch (currentLedMode) {
    case LED_NORMAL: ledText = "NORMAL"; break;
    case LED_EMERGENCY: ledText = "EMERGENCY"; break;
    case LED_CONTACT_CLOSED: ledText = "CONTACT_CLOSED"; break;
    case LED_BYPASS: ledText = "BYPASS"; break;
    default: break;
  }
  doc["ledMode"] = ledText;
  doc["ledOn"] = ledOutputState;
  doc["faultMessage"] = faultMessage;
  if (WiFi.status() == WL_CONNECTED) {
    doc["mac"] = WiFi.macAddress();
  } else {
    doc["mac"] = WiFi.softAPmacAddress();
  }
  // EEPROM'daki günlük enerji logunu ekle (15 sn'de bir CRM'e gider)
  JsonArray historyArr = doc.createNestedArray("energyHistory");
  for (size_t i = 0; i < energyHistoryCount; i++) {
    JsonObject rec = historyArr.add<JsonObject>();
    rec["year"] = energyHistory[i].year;
    rec["month"] = energyHistory[i].month;
    rec["day"] = energyHistory[i].day;
    rec["energyIn"] = energyHistory[i].energyIn;
    rec["energyOut"] = energyHistory[i].energyOut;
  }
  String out;
  serializeJson(doc, out);
  return out;
}

String getCrmBaseUrl() {
  String u = config.crmUrl;
  u.trim();
  while (u.endsWith("/")) u.remove(u.length() - 1);
  // http:// verilmişse otomatik olarak https:// yap (Vercel sadece HTTPS destekler)
  if (u.startsWith("http://")) {
    u = "https://" + u.substring(7);
  }
  return u;
}

String extractHost(const String &baseUrl) {
  int schemeEnd = baseUrl.indexOf("://");
  if (schemeEnd < 0) return "";
  int hostStart = schemeEnd + 3;
  int hostEnd = baseUrl.indexOf("/", hostStart);
  if (hostEnd < 0) hostEnd = baseUrl.length();
  String hostPort = baseUrl.substring(hostStart, hostEnd);
  int colon = hostPort.indexOf(":");
  return (colon >= 0) ? hostPort.substring(0, colon) : hostPort;
}

bool checkDns(const String &hostname) {
  IPAddress ip;
  if (WiFi.hostByName(hostname.c_str(), ip)) {
    Serial.printf("DNS OK: %s -> %s\n", hostname.c_str(), ip.toString().c_str());
    return true;
  }
  Serial.printf("DNS FAIL: %s could not be resolved! DNS=%s\n",
    hostname.c_str(), WiFi.dnsIP().toString().c_str());
  return false;
}

void pushTelemetryToCrm() {
  if (config.crmUrl[0] == '\0' || config.crmApiKey[0] == '\0') return;
  String baseUrl = getCrmBaseUrl();
  if (baseUrl.length() < 10) return;

  String host = extractHost(baseUrl);
  if (host.length() == 0) return;

  String body = buildStatusJsonString();
  String url = baseUrl + "/api/bms/telemetry";

  Serial.printf("CRM push: %s (heap: %u)\n", url.c_str(), ESP.getFreeHeap());

  if (!checkDns(host)) return;

  HTTPClient http;
  WiFiClientSecure *secClient = new WiFiClientSecure;
  if (!secClient) { Serial.println("CRM push: out of memory"); return; }
  secClient->setInsecure();
  secClient->setHandshakeTimeout(30);
  http.begin(*secClient, url);

  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setRedirectLimit(5);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-BMS-API-Key", config.crmApiKey);
  http.setTimeout(2500);

  int httpCode = http.POST(body);
  if (httpCode > 0) {
    Serial.printf("CRM push OK: %d\n", httpCode);
  } else {
    Serial.printf("CRM push FAIL(%d): %s\n", httpCode, http.errorToString(httpCode).c_str());
  }

  http.end();
  delete secClient;
}

void processCrmCommands() {
  if (crmStartPending) {
    crmStartPending = false;
    startSequence();
    Serial.println("CRM: start sequence executed");
  }
  if (crmEmergencyPending) {
    crmEmergencyPending = false;
    triggerEmergencyStop(true);
    Serial.println("CRM: emergency stop executed");
  }
  while (crmButtonQueueHead != crmButtonQueueTail) {
    unsigned long dur = crmButtonQueue[crmButtonQueueHead];
    crmButtonQueueHead = (crmButtonQueueHead + 1) % CRM_BUTTON_QUEUE_SIZE;
    triggerVirtualButtonPress(dur);
    Serial.printf("CRM: button press executed (%lu ms)\n", dur);
  }
}

String normalizeVersionTag(const String &versionTag) {
  String normalized = versionTag;
  normalized.trim();
  while (normalized.startsWith("v") || normalized.startsWith("V")) {
    normalized.remove(0, 1);
  }
  return normalized;
}

int compareVersionStrings(const String &a, const String &b) {
  int a1 = 0, a2 = 0, a3 = 0;
  int b1 = 0, b2 = 0, b3 = 0;
  sscanf(a.c_str(), "%d.%d.%d", &a1, &a2, &a3);
  sscanf(b.c_str(), "%d.%d.%d", &b1, &b2, &b3);

  if (a1 != b1) return (a1 > b1) ? 1 : -1;
  if (a2 != b2) return (a2 > b2) ? 1 : -1;
  if (a3 != b3) return (a3 > b3) ? 1 : -1;
  return 0;
}

String getOtaGithubToken() {
  String token = String(OTA_GITHUB_TOKEN);
  if (token.length() > 0) {
    return token;
  }

  token = String(OTA_GITHUB_TOKEN_PART1);
  token += OTA_GITHUB_TOKEN_PART2;
  return token;
}

bool downloadAndApplyOtaAsset(const String &assetApiUrl, int updateCommand, const String &assetLabel, bool restartAfter) {
  String otaToken = getOtaGithubToken();
  if (otaToken.length() == 0) {
    Serial.println("OTA: GitHub token is empty");
    return false;
  }

  HTTPClient http;
  WiFiClientSecure *secClient = new WiFiClientSecure;
  if (!secClient) {
    Serial.println("OTA: no memory for secure client");
    return false;
  }

  secClient->setInsecure();
  secClient->setHandshakeTimeout(30);
  http.begin(*secClient, assetApiUrl);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setRedirectLimit(5);
  http.setTimeout(30000);
  http.addHeader("Authorization", String("Bearer ") + otaToken);
  http.addHeader("User-Agent", "Nama-BMS-ESP32");
  http.addHeader("Accept", "application/vnd.github.raw");

  Serial.printf("OTA: %s download started\n", assetLabel.c_str());

  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.printf("OTA: %s download HTTP %d\n", assetLabel.c_str(), httpCode);
    http.end();
    delete secClient;
    return false;
  }

  int contentLength = http.getSize();
  bool spiffsUnmounted = false;
  if (updateCommand == U_SPIFFS) {
    SPIFFS.end();
    spiffsUnmounted = true;
  }

  if (!Update.begin(contentLength > 0 ? contentLength : UPDATE_SIZE_UNKNOWN, updateCommand)) {
    Serial.printf("OTA: Update.begin failed (%s)\n", Update.errorString());
    if (spiffsUnmounted) {
      SPIFFS.begin(true);
    }
    http.end();
    delete secClient;
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);
  if (written == 0) {
    Serial.printf("OTA: write failed (%s)\n", Update.errorString());
    Update.abort();
    if (spiffsUnmounted) {
      SPIFFS.begin(true);
    }
    http.end();
    delete secClient;
    return false;
  }

  if (!Update.end()) {
    Serial.printf("OTA: finalize failed (%s)\n", Update.errorString());
    if (spiffsUnmounted) {
      SPIFFS.begin(true);
    }
    http.end();
    delete secClient;
    return false;
  }

  if (!Update.isFinished()) {
    Serial.println("OTA: not finished");
    if (spiffsUnmounted) {
      SPIFFS.begin(true);
    }
    http.end();
    delete secClient;
    return false;
  }

  http.end();
  delete secClient;

  Serial.printf("OTA: %s update applied successfully\n", assetLabel.c_str());

  if (restartAfter) {
    Serial.println("OTA: restart requested, rebooting now...");
    delay(1000);
    ESP.restart();
  }

  return true;
}

void checkForOtaUpdate() {
  if (WiFi.status() != WL_CONNECTED) return;
  String otaToken = getOtaGithubToken();
  if (otaToken.length() == 0) return;

  String manifestUrl = String("https://api.github.com/repos/") + OTA_GITHUB_OWNER + "/" + OTA_GITHUB_REPO + "/contents/" + OTA_MANIFEST_PATH + "?ref=main";

  HTTPClient http;
  WiFiClientSecure *secClient = new WiFiClientSecure;
  if (!secClient) return;

  secClient->setInsecure();
  secClient->setHandshakeTimeout(30);
  http.begin(*secClient, manifestUrl);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setRedirectLimit(5);
  http.setTimeout(8000);
  http.addHeader("Authorization", String("Bearer ") + otaToken);
  http.addHeader("User-Agent", "Nama-BMS-ESP32");
  http.addHeader("Accept", "application/vnd.github.raw");

  int httpCode = http.GET();
  if (httpCode != 200) {
    if (httpCode > 0) {
      Serial.printf("OTA: manifest HTTP %d\n", httpCode);
    } else {
      Serial.printf("OTA: manifest fail %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
    delete secClient;
    return;
  }

  String body = http.getString();
  http.end();
  delete secClient;

  DynamicJsonDocument doc(6144);
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    Serial.println("OTA: manifest parse error");
    return;
  }

  String targetVersion = normalizeVersionTag(doc["version"] | "");
  String currentVersion = normalizeVersionTag(String(FW_VERSION));
  bool force = doc["force"] | false;

  if (targetVersion.length() == 0) {
    Serial.println("OTA: manifest version missing");
    return;
  }

  if (!force && compareVersionStrings(targetVersion, currentVersion) <= 0) {
    return;
  }

  String fwAssetApiUrl = doc["firmwareApiUrl"] | "";
  String spiffsAssetApiUrl = doc["spiffsApiUrl"] | "";

  String repoBase = String("https://api.github.com/repos/") + OTA_GITHUB_OWNER + "/" + OTA_GITHUB_REPO + "/contents/";
  if (fwAssetApiUrl.length() == 0) {
    fwAssetApiUrl = repoBase + "ota/" + OTA_FW_ASSET_NAME + "?ref=main";
  }
  if (spiffsAssetApiUrl.length() == 0) {
    spiffsAssetApiUrl = repoBase + "ota/" + OTA_SPIFFS_ASSET_NAME + "?ref=main";
  }

  if (fwAssetApiUrl.length() == 0 && spiffsAssetApiUrl.length() == 0) {
    Serial.printf("OTA: asset not found (%s / %s)\n", OTA_FW_ASSET_NAME, OTA_SPIFFS_ASSET_NAME);
    return;
  }

  Serial.printf("OTA: update available %s -> %s\n", currentVersion.c_str(), targetVersion.c_str());

  bool spiffsUpdated = false;
  if (spiffsAssetApiUrl.length() > 0) {
    Serial.println("OTA: SPIFFS update detected");
    spiffsUpdated = downloadAndApplyOtaAsset(spiffsAssetApiUrl, U_SPIFFS, "SPIFFS", false);
    if (!spiffsUpdated) {
      Serial.println("OTA: SPIFFS update failed");
      return;
    }
  }

  if (fwAssetApiUrl.length() > 0) {
    Serial.println("OTA: Firmware update detected");
    if (!downloadAndApplyOtaAsset(fwAssetApiUrl, U_FLASH, "Firmware", true)) {
      Serial.println("OTA: Firmware update failed");
    }
    return;
  }

  if (spiffsUpdated) {
    Serial.println("OTA: only SPIFFS updated, rebooting to remount FS...");
    delay(1000);
    ESP.restart();
  }
}

void crmTask(void *param) {
  unsigned long lastPush = 0;
  unsigned long lastPoll = 0;
  unsigned long lastOtaCheck = 0;
  bool networkReady = false;

  for (;;) {
    vTaskDelay(50 / portTICK_PERIOD_MS);

    if (WiFi.status() != WL_CONNECTED) {
      networkReady = false;
      continue;
    }

    unsigned long now = millis();
    bool crmReady = config.crmUrl[0] != '\0' && config.crmApiKey[0] != '\0';

    if (!networkReady) {
      networkReady = true;
      lastPush = now - CRM_PUSH_INTERVAL_MS;
      lastPoll = now - CRM_POLL_INTERVAL_MS;
      lastOtaCheck = now - OTA_CHECK_INTERVAL_MS;
      Serial.printf("Network ready (Core %d). Heap: %u, DNS: %s\n",
        xPortGetCoreID(), ESP.getFreeHeap(), WiFi.dnsIP().toString().c_str());
    }

    if (crmReady && now - lastPoll >= CRM_POLL_INTERVAL_MS) {
      lastPoll = now;
      pollCommandsFromCrm();
    }

    if (crmReady && now - lastPush >= CRM_PUSH_INTERVAL_MS) {
      lastPush = now;
      pushTelemetryToCrm();
    }

    // Beklenmedik restart olmamasi icin OTA sadece manuel tetikleme ile calissin.
    // Manuel tetikleme: /api/ota-check
    if (otaCheckPending) {
      lastOtaCheck = now;
      otaCheckPending = false;
      checkForOtaUpdate();
    }
  }
}

void pollCommandsFromCrm() {
  if (config.crmUrl[0] == '\0' || config.crmApiKey[0] == '\0') return;
  String baseUrl = getCrmBaseUrl();
  if (baseUrl.length() < 10) return;

  String mac = (WiFi.status() == WL_CONNECTED) ? WiFi.macAddress() : WiFi.softAPmacAddress();
  String url = baseUrl + "/api/bms/commands?mac=" + mac;

  HTTPClient http;
  WiFiClientSecure *secClient = nullptr;

  if (baseUrl.startsWith("https://")) {
    secClient = new WiFiClientSecure;
    if (!secClient) return;
    secClient->setInsecure();
    secClient->setHandshakeTimeout(30);
    http.begin(*secClient, url);
  } else {
    http.begin(url);
  }

  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setRedirectLimit(5);
  http.addHeader("X-BMS-API-Key", config.crmApiKey);
  http.setTimeout(700);

  int httpCode = http.GET();

  if (httpCode == 200) {
    String responseBody = http.getString();

    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, responseBody) == DeserializationError::Ok) {
      JsonArray arr = doc["commands"].as<JsonArray>();
      if (!arr.isNull()) {
        for (JsonObject cmd : arr) {
          const char *type = cmd["commandType"];
          if (!type) continue;
          if (strcmp(type, "start") == 0) {
            crmStartPending = true;
          } else if (strcmp(type, "emergency") == 0) {
            crmEmergencyPending = true;
          } else if (strcmp(type, "button") == 0) {
            JsonObject pl = cmd["payload"].as<JsonObject>();
            unsigned long dur = pl["durationMs"] | 120UL;
            enqueueCrmButton(dur);
          } else if (strcmp(type, "config") == 0) {
            JsonObject pl = cmd["payload"].as<JsonObject>();
            if (!pl.isNull()) {
              if (pl.containsKey("batteryType")) config.batteryType = pl["batteryType"] | config.batteryType;
              if (pl.containsKey("seriesCount")) config.seriesCount = pl["seriesCount"] | config.seriesCount;
              if (pl.containsKey("cellCapacity")) config.cellCapacity = pl["cellCapacity"] | config.cellCapacity;
              if (pl.containsKey("maxChargeCurrent")) config.maxChargeCurrent = pl["maxChargeCurrent"] | config.maxChargeCurrent;
              if (pl.containsKey("maxDischargeCurrent")) config.maxDischargeCurrent = pl["maxDischargeCurrent"] | config.maxDischargeCurrent;
              if (pl.containsKey("prechargeTime")) config.prechargeTime = pl["prechargeTime"] | config.prechargeTime;
              if (pl.containsKey("mosfetOnTime")) config.mosfetOnTime = pl["mosfetOnTime"] | config.mosfetOnTime;
              if (pl.containsKey("shortCircuitThreshold")) config.shortCircuitThreshold = pl["shortCircuitThreshold"] | config.shortCircuitThreshold;
              if (pl.containsKey("sensor1Calibration")) config.sensor1Calibration = pl["sensor1Calibration"] | config.sensor1Calibration;
              if (pl.containsKey("sensor2Calibration")) config.sensor2Calibration = pl["sensor2Calibration"] | config.sensor2Calibration;
              saveConfig();
            }
          }
        }
      }
    }
  } else if (httpCode > 0) {
    Serial.printf("CRM poll HTTP %d\n", httpCode);
  } else {
    static unsigned long lastPollErrLog = 0;
    if (millis() - lastPollErrLog > 15000) {
      lastPollErrLog = millis();
      Serial.printf("CRM poll FAIL: %s -> %s\n", http.errorToString(httpCode).c_str(), url.c_str());
    }
  }

  http.end();
  if (secClient) delete secClient;
}

