/*
  ESP32 Smart Watering System
  Fully compatible with ESP32 DevKit V1 (ESP32-WROOM-32)
  Works with Arduino-ESP32 core 3.0+ & NimBLE-Arduino 1.4+
  Features: Soil moisture threshold, Daily schedule, Manual BLE control, Autotune, Tank level, PWM/Relay
*/

#include <Arduino.h>
#include <Preferences.h>
#include <NimBLEDevice.h>

// ========================= PINS (ESP32 DevKit V1) =========================
#define SOIL_PIN      34   // ADC1_CH6 - Capacitive soil sensor
#define LEVEL_PIN     35   // ADC1_CH7 - Water tank level sensor (optional)
#define RELAY_PIN     26   // Relay control (active LOW typical)
#define PUMP_PWM_PIN  25   // Optional PWM for MOSFET/dimmer (if PWM_MODE = true)

// ========================= MODES =========================
const bool RELAY_MODE = true;   // Set false if using MOSFET + PWM
const bool PWM_MODE   = false;

// ========================= CONFIG =========================
Preferences prefs;

struct Config {
  uint16_t threshold    = 2200;   // Dry threshold (higher = drier, typical 1500-3000)
  uint8_t  flowPct      = 100;    // PWM duty 0-100%
  uint8_t  startHour    = 7;
  uint8_t  startMin     = 0;
  uint16_t durationSec  = 30;     // Watering duration in seconds
  bool     scheduleOn   = false;
  bool     relayActiveLow = true; // Most relay modules are active LOW
} cfg;

// ========================= SAVE / LOAD =========================
void saveConfig() {
  prefs.begin("watering", false);
  prefs.putUShort("th", cfg.threshold);
  prefs.putUChar ("fp", cfg.flowPct);
  prefs.putUChar ("sh", cfg.startHour);
  prefs.putUChar ("sm", cfg.startMin);
  prefs.putUShort("dur", cfg.durationSec);
  prefs.putBool  ("sch", cfg.scheduleOn);
  prefs.putBool  ("inv", cfg.relayActiveLow);
  prefs.end();
}

void loadConfig() {
  prefs.begin("watering", true);
  cfg.threshold     = prefs.getUShort("th", cfg.threshold);
  cfg.flowPct       = prefs.getUChar ("fp", cfg.flowPct);
  cfg.startHour     = prefs.getUChar ("sh", cfg.startHour);
  cfg.startMin      = prefs.getUChar ("sm", cfg.startMin);
  cfg.durationSec   = prefs.getUShort("dur", cfg.durationSec);
  cfg.scheduleOn    = prefs.getBool  ("sch", cfg.scheduleOn);
  cfg.relayActiveLow = prefs.getBool ("inv", cfg.relayActiveLow);
  prefs.end();
}

// ========================= BLE UUIDs =========================
#define SERVICE_UUID        "e0b30000-9f3c-4b1c-a9a4-9a1dfb2a9c01"
#define CHR_SOIL            "e0b30001-9f3c-4b1c-a9a4-9a1dfb2a9c01"
#define CHR_LEVEL           "e0b30002-9f3c-4b1c-a9a4-9a1dfb2a9c01"
#define CHR_STATE           "e0b30003-9f3c-4b1c-a9a4-9a1dfb2a9c01"  // [pumpOn, flow%]
#define CHR_CONFIG          "e0b30004-9f3c-4b1c-a9a4-9a1dfb2a9c01"  // 10-byte packed
#define CHR_COMMAND         "e0b30005-9f3c-4b1c-a9a4-9a1dfb2a9c01"  // text commands
#define CHR_LOG             "e0b30006-9f3c-4b1c-a9a4-9a1dfb2a9c01"

NimBLEServer* pServer;
NimBLECharacteristic* pSoilChr;
NimBLECharacteristic* pLevelChr;
NimBLECharacteristic* pStateChr;
NimBLECharacteristic* pConfigChr;
NimBLECharacteristic* pCmdChr;
NimBLECharacteristic* pLogChr;

bool pumpRunning = false;
bool clientConnected = false;

// ========================= PACK / UNPACK CONFIG =========================
std::string packConfig() {
  uint8_t data[10];
  data[0] = cfg.threshold & 0xFF;
  data[1] = cfg.threshold >> 8;
  data[2] = cfg.flowPct;
  data[3] = cfg.startHour;
  data[4] = cfg.startMin;
  data[5] = cfg.durationSec & 0xFF;
  data[6] = cfg.durationSec >> 8;
  data[7] = cfg.scheduleOn ? 1 : 0;
  data[8] = cfg.relayActiveLow ? 1 : 0;
  data[9] = 0xAA; // version marker
  return std::string((char*)data, 10);
}

void unpackConfig(const uint8_t* data, size_t len) {
  if (len < 10) return;
  cfg.threshold     = data[0] | (data[1] << 8);
  cfg.flowPct       = data[2];
  cfg.startHour     = data[3];
  cfg.startMin      = data[4];
  cfg.durationSec   = data[5] | (data[6] << 8);
  cfg.scheduleOn    = data[7];
  cfg.relayActiveLow = data[8];
  saveConfig();
}

// ========================= LOGGING =========================
void log(const String& msg) {
  Serial.println(msg);
  if (pLogChr && clientConnected) {
    pLogChr->setValue(msg.c_str());
    pLogChr->notify();
  }
}

// ========================= PUMP CONTROL =========================
void setPump(bool on) {
  pumpRunning = on;

  if (RELAY_MODE) {
    digitalWrite(RELAY_PIN, cfg.relayActiveLow ? !on : on);
  }
  if (PWM_MODE) {
    uint8_t duty = on ? map(cfg.flowPct, 0, 100, 0, 255) : 0;
    ledcWrite(0, duty);
  }

  uint8_t state[2] = { on ? 1 : 0, cfg.flowPct };
  pStateChr->setValue(state, 2);
  pStateChr->notify();

  log(on ? "Pump ON" : "Pump OFF");
}

// ========================= BLE CALLBACKS (2025 API) =========================
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    clientConnected = true;
    log("BLE Client connected");
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    clientConnected = false;
    log("BLE Client disconnected - Restarting advertising");
    NimBLEDevice::startAdvertising();
  }
};

class CmdCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChr, NimBLEConnInfo& connInfo) override {
    std::string value = pChr->getValue();
    String cmd = String(value.c_str()).c_str();

    if (cmd == "ON") {
      setPump(true);
    } else if (cmd == "OFF") {
      setPump(false);
    } else if (cmd == "TOGGLE") {
      setPump(!pumpRunning);
    } else if (cmd.startsWith("WATER:")) {
      int sec = cmd.substring(6).toInt();
      if (sec <= 0) sec = cfg.durationSec;
      setPump(true);
      delay(sec * 1000);
      setPump(false);
      log("Manual watering finished");
    } else if (cmd.startsWith("AUTOTUNE")) {
      log("Autotune started (10 sec dry reading)...");
      delay(2000);
      uint32_t sum = 0; int n = 0;
      uint32_t t0 = millis();
      while (millis() - t0 < 10000) {
        sum += analogRead(SOIL_PIN);
        n++;
        delay(100);
      }
      if (n > 0) {
        uint16_t avg = sum / n;
        cfg.threshold = avg + 200;  // dry = higher reading
        saveConfig();
        log(String("Autotune done → new threshold = ") + cfg.threshold);
        pConfigChr->setValue(packConfig());
        pConfigChr->notify();
      }
    } else {
      log("Unknown command: " + cmd);
    }
  }
};

class ConfigCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pChr, NimBLEConnInfo& connInfo) override {
    pChr->setValue(packConfig());
  }
  void onWrite(NimBLECharacteristic* pChr, NimBLEConnInfo& connInfo) override {
    std::string v = pChr->getValue();
    unpackConfig((uint8_t*)v.data(), v.length());
    log("Config updated from app");
  }
};

// ========================= PWM SETUP =========================
void setupPWM() {
  if (PWM_MODE) {
    ledcAttach(PUMP_PWM_PIN, 1000, 8);  // New API (Arduino-ESP32 ≥2.0.14)
    ledcWrite(PUMP_PWM_PIN, 0);
  }
}

// ========================= VARIABLES =========================
uint32_t lastSoilTime = 0;
uint32_t lastLevelTime = 0;
uint32_t secondsToday = 0;
uint32_t lastSecond = 0;
bool wateredToday = false;

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, cfg.relayActiveLow ? HIGH : LOW);

  loadConfig();
  setupPWM();

  NimBLEDevice::init("Smart Watering");
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  pSoilChr   = pService->createCharacteristic(CHR_SOIL,   NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  pLevelChr  = pService->createCharacteristic(CHR_LEVEL,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  pStateChr  = pService->createCharacteristic(CHR_STATE, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  pConfigChr = pService->createCharacteristic(CHR_CONFIG,NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  pCmdChr    = pService->createCharacteristic(CHR_COMMAND,NIMBLE_PROPERTY::WRITE);
  pLogChr    = pService->createCharacteristic(CHR_LOG,   NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  pConfigChr->setCallbacks(new ConfigCallbacks());
  pCmdChr->setCallbacks(new CmdCallbacks());

  pService->start();

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setAppearance(0x00);
  pAdvertising->start();

  log("Smart Watering System Ready!");
}

void loop() {
  // One-second ticker
  if (millis() - lastSecond >= 1000) {
    lastSecond = millis();
    secondsToday = (secondsToday + 1) % 86400;
    if (secondsToday < 10) wateredToday = false; // midnight reset
  }

  // Soil moisture reading & notify
  if (millis() - lastSoilTime > 1000) {
    uint16_t soil = analogRead(SOIL_PIN);
    uint8_t b[2] = { lowByte(soil), highByte(soil) };
    pSoilChr->setValue(b, 2);
    pSoilChr->notify();
    lastSoilTime = millis();

    // Auto watering when soil is dry (and schedule is OFF)
    if (!cfg.scheduleOn && soil > cfg.threshold && !pumpRunning) {
      setPump(true);
      delay(cfg.durationSec * 1000);
      setPump(false);
      log("Auto watering completed");
    }
  }

  // Tank level
  if (millis() - lastLevelTime > 2000) {
    uint16_t level = analogRead(LEVEL_PIN);
    uint8_t b[2] = { lowByte(level), highByte(level) };
    pLevelChr->setValue(b, 2);
    pLevelChr->notify();
    lastLevelTime = millis();
  }

  // Daily scheduled watering
  if (cfg.scheduleOn) {
    uint32_t nowSec = cfg.startHour * 3600 + cfg.startMin * 60;
    if (!wateredToday && secondsToday >= nowSec && secondsToday < nowSec + 20) {
      setPump(true);
      delay(cfg.durationSec * 1000);
      setPump(false);
      wateredToday = true;
      log("Scheduled watering done");
    }
  }

  delay(50);
}