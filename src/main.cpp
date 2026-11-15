#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "app_state.h"
#include "board_pins.h"
#include "ui.h"

namespace {

constexpr uint32_t kTouchCalMagic = 0xF16C5A21;
constexpr uint32_t kGpsBaud = 9600;
constexpr uint32_t kWifiDwellMs = 110;
constexpr uint32_t kBleScanSeconds = 3;
constexpr uint32_t kGpsFreshMs = 4000;
constexpr uint32_t kUiRefreshMs = 250;
constexpr uint32_t kLogRefreshMs = 1000;
constexpr uint32_t kSdSpiHz = 8000000;
constexpr char kObservationPath[] = "/wardriver.csv";

struct TouchCalibration {
  uint32_t magic = kTouchCalMagic;
  float x[3] = {0.0f, 1.0f, 0.0f};
  float y[3] = {0.0f, 0.0f, 1.0f};
};

struct RawTouch {
  uint16_t x = 0;
  uint16_t y = 0;
  uint16_t z1 = 0;
  uint16_t z2 = 0;
  bool down = false;
};

BoardPins pins;
AppState app;
WardriverUI ui;
HardwareSerial gpsUart(1);
Preferences prefs;
BLEScan *bleScan = nullptr;
TouchCalibration touchCalibration;

char nmeaLine[128] = {};
size_t nmeaLength = 0;
bool wifiScanPending = false;
bool scanGateOpen = false;
bool touchWasDown = false;
uint32_t touchDownAt = 0;
uint16_t touchDownX = 0;
uint16_t touchDownY = 0;
uint32_t lastUiDraw = 0;
uint32_t lastLogRefresh = 0;
uint32_t lastScanRequest = 0;
uint8_t cueMode = 0;

bool endsWithIgnoreCase(const char *text, const char *suffix) {
  if (!text || !suffix) return false;
  const size_t n = strlen(text);
  const size_t m = strlen(suffix);
  if (m > n) return false;
  const char *tail = text + n - m;
  for (size_t i = 0; i < m; ++i) {
    if (tolower(static_cast<unsigned char>(tail[i])) !=
        tolower(static_cast<unsigned char>(suffix[i]))) return false;
  }
  return true;
}

bool privacyExcluded(const char *name) {
  return app.honorNomap &&
         (endsWithIgnoreCase(name, "_nomap") || endsWithIgnoreCase(name, "_optout"));
}

int findContact(ContactProtocol protocol, const char *address) {
  for (uint8_t i = 0; i < app.contactCount; ++i) {
    if (app.contacts[i].protocol == protocol &&
        strncmp(app.contacts[i].address, address, sizeof(app.contacts[i].address)) == 0) {
      return i;
    }
  }
  return -1;
}

int upsertContact(ContactProtocol protocol, const char *name, const char *address,
                  int rssiDbm, uint8_t channel, WifiSecurity security) {
  if (privacyExcluded(name)) return -1;

  int index = findContact(protocol, address);
  if (index < 0) {
    if (app.contactCount >= kMaxContacts) return -1;
    index = app.contactCount++;
    app.contacts[index] = ContactObservation{};
    app.contacts[index].protocol = protocol;
    app.contacts[index].sightings = 0;
    strlcpy(app.contacts[index].address, address, sizeof(app.contacts[index].address));
  }

  ContactObservation &entry = app.contacts[index];
  if (name && name[0]) strlcpy(entry.name, name, sizeof(entry.name));
  else if (!entry.name[0]) strlcpy(entry.name, address, sizeof(entry.name));
  entry.rssiDbm = static_cast<int8_t>(constrain(rssiDbm, -127, 0));
  entry.channel = channel;
  entry.security = security;
  if (entry.sightings < UINT16_MAX) ++entry.sightings;
  entry.lastSeenMs = millis();
  return index;
}

double nmeaCoordinate(const char *value, const char *hemisphere) {
  if (!value || !value[0]) return 0.0;
  const double raw = atof(value);
  const double degrees = floor(raw / 100.0);
  double decimal = degrees + (raw - degrees * 100.0) / 60.0;
  if (hemisphere && (hemisphere[0] == 'S' || hemisphere[0] == 'W')) decimal = -decimal;
  return decimal;
}

int splitNmea(char *line, char **fields, int capacity) {
  int count = 0;
  char *save = nullptr;
  for (char *token = strtok_r(line, ",", &save); token && count < capacity;
       token = strtok_r(nullptr, ",", &save)) {
    char *star = strchr(token, '*');
    if (star) *star = '\0';
    fields[count++] = token;
  }
  return count;
}

void parseNmeaSentence(char *line) {
  if (!line || line[0] != '$') return;
  char *fields[24] = {};
  const int count = splitNmea(line, fields, 24);
  if (count < 2) return;
  const uint32_t now = millis();

  if (strstr(fields[0], "GGA") && count > 8) {
    app.gps.receiverDetected = true;
    app.gps.receiverConfigured = true;
    app.gps.lastSentenceMs = now;
    const int fixQuality = atoi(fields[6]);
    app.gps.hasFix = fixQuality > 0;
    app.gps.satellites = static_cast<uint8_t>(constrain(atoi(fields[7]), 0, 99));
    app.gps.hdop = atof(fields[8]);
    if (app.gps.hasFix) {
      app.gps.latitude = nmeaCoordinate(fields[2], fields[3]);
      app.gps.longitude = nmeaCoordinate(fields[4], fields[5]);
    }
  } else if (strstr(fields[0], "RMC") && count > 8) {
    app.gps.receiverDetected = true;
    app.gps.receiverConfigured = true;
    app.gps.lastSentenceMs = now;
    app.gps.hasFix = fields[2][0] == 'A';
    if (app.gps.hasFix) {
      app.gps.latitude = nmeaCoordinate(fields[3], fields[4]);
      app.gps.longitude = nmeaCoordinate(fields[5], fields[6]);
      app.gps.speedKmh = atof(fields[7]) * 1.852f;
      app.gps.courseDegrees = atof(fields[8]);
    }
  }
}

void pollGps() {
  while (gpsUart.available()) {
    const char c = static_cast<char>(gpsUart.read());
    if (c == '\r') continue;
    if (c == '\n') {
      nmeaLine[nmeaLength] = '\0';
      parseNmeaSentence(nmeaLine);
      nmeaLength = 0;
    } else if (nmeaLength + 1 < sizeof(nmeaLine)) {
      nmeaLine[nmeaLength++] = c;
    } else {
      nmeaLength = 0;
    }
  }

  if (app.gps.receiverDetected && millis() - app.gps.lastSentenceMs > kGpsFreshMs) {
    app.gps.receiverDetected = false;
    app.gps.hasFix = false;
  }
}

uint16_t xptRead(uint8_t command) {
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(pins.touchCs, LOW);
  SPI.transfer(command);
  const uint16_t value = static_cast<uint16_t>(SPI.transfer(0x00)) << 8;
  const uint16_t low = SPI.transfer(0x00);
  digitalWrite(pins.touchCs, HIGH);
  SPI.endTransaction();
  return ((value | low) >> 3) & 0x0FFF;
}

RawTouch readRawTouch() {
  RawTouch raw;
  raw.z1 = xptRead(0xB0);
  raw.z2 = xptRead(0xC0);
  raw.x = xptRead(0xD0);
  raw.y = xptRead(0x90);
  const int pressure = static_cast<int>(raw.z1) + 4095 - static_cast<int>(raw.z2);
  raw.down = raw.z1 > 0 && pressure > 180;
  return raw;
}

bool solve3x3(float matrix[3][4], float result[3]) {
  for (int col = 0; col < 3; ++col) {
    int pivot = col;
    for (int row = col + 1; row < 3; ++row) {
      if (fabsf(matrix[row][col]) > fabsf(matrix[pivot][col])) pivot = row;
    }
    if (fabsf(matrix[pivot][col]) < 0.00001f) return false;
    if (pivot != col) {
      for (int j = col; j < 4; ++j) {
        const float tmp = matrix[col][j];
        matrix[col][j] = matrix[pivot][j];
        matrix[pivot][j] = tmp;
      }
    }
    const float divisor = matrix[col][col];
    for (int j = col; j < 4; ++j) matrix[col][j] /= divisor;
    for (int row = 0; row < 3; ++row) {
      if (row == col) continue;
      const float factor = matrix[row][col];
      for (int j = col; j < 4; ++j) matrix[row][j] -= factor * matrix[col][j];
    }
  }
  for (int i = 0; i < 3; ++i) result[i] = matrix[i][3];
  return true;
}

bool fitAffine(const uint16_t rawX[4], const uint16_t rawY[4],
               const uint16_t targetX[4], const uint16_t targetY[4],
               TouchCalibration &calibration) {
  float normal[3][3] = {};
  float bx[3] = {};
  float by[3] = {};
  for (int i = 0; i < 4; ++i) {
    const float v[3] = {static_cast<float>(rawX[i]), static_cast<float>(rawY[i]), 1.0f};
    for (int r = 0; r < 3; ++r) {
      bx[r] += v[r] * targetX[i];
      by[r] += v[r] * targetY[i];
      for (int c = 0; c < 3; ++c) normal[r][c] += v[r] * v[c];
    }
  }
  float matrixX[3][4] = {};
  float matrixY[3][4] = {};
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      matrixX[r][c] = normal[r][c];
      matrixY[r][c] = normal[r][c];
    }
    matrixX[r][3] = bx[r];
    matrixY[r][3] = by[r];
  }
  float outX[3], outY[3];
  if (!solve3x3(matrixX, outX) || !solve3x3(matrixY, outY)) return false;
  memcpy(calibration.x, outX, sizeof(outX));
  memcpy(calibration.y, outY, sizeof(outY));
  calibration.magic = kTouchCalMagic;
  return true;
}

bool loadTouchCalibration() {
  prefs.begin("wardriver", true);
  const size_t size = prefs.getBytesLength("touchcal");
  const bool valid = size == sizeof(touchCalibration) &&
                     prefs.getBytes("touchcal", &touchCalibration, sizeof(touchCalibration)) == sizeof(touchCalibration) &&
                     touchCalibration.magic == kTouchCalMagic;
  prefs.end();
  app.touchCalibrated = valid;
  return valid;
}

void saveTouchCalibration() {
  prefs.begin("wardriver", false);
  prefs.putBytes("touchcal", &touchCalibration, sizeof(touchCalibration));
  prefs.end();
  app.touchCalibrated = true;
}

bool calibrateTouch() {
  constexpr uint8_t kPoints = 4;
  const uint16_t targetX[kPoints] = {28, 452, 452, 28};
  const uint16_t targetY[kPoints] = {28, 28, 292, 292};
  uint16_t rawX[kPoints] = {};
  uint16_t rawY[kPoints] = {};

  for (uint8_t point = 0; point < kPoints; ++point) {
    ui.drawCalibrationTarget(point, kPoints, targetX[point], targetY[point]);
    uint32_t started = millis();
    while (millis() - started < 30000) {
      RawTouch raw = readRawTouch();
      if (!raw.down) {
        delay(10);
        continue;
      }
      uint32_t totalX = 0, totalY = 0;
      uint8_t samples = 0;
      for (uint8_t n = 0; n < 8; ++n) {
        raw = readRawTouch();
        if (raw.down) {
          totalX += raw.x;
          totalY += raw.y;
          ++samples;
        }
        delay(8);
      }
      if (samples >= 4) {
        rawX[point] = totalX / samples;
        rawY[point] = totalY / samples;
        while (readRawTouch().down) delay(10);
        break;
      }
    }
    if (millis() - started >= 30000) return false;
  }

  if (!fitAffine(rawX, rawY, targetX, targetY, touchCalibration)) return false;
  saveTouchCalibration();
  return true;
}

bool readCalibratedTouch(uint16_t &x, uint16_t &y) {
  const RawTouch raw = readRawTouch();
  if (!raw.down) return false;
  const float xf = touchCalibration.x[0] * raw.x + touchCalibration.x[1] * raw.y + touchCalibration.x[2];
  const float yf = touchCalibration.y[0] * raw.x + touchCalibration.y[1] * raw.y + touchCalibration.y[2];
  x = static_cast<uint16_t>(constrain(static_cast<int>(lroundf(xf)), 0, 479));
  y = static_cast<uint16_t>(constrain(static_cast<int>(lroundf(yf)), 0, 319));
  return true;
}

WifiSecurity securityFromAuth(wifi_auth_mode_t auth) {
  switch (auth) {
    case WIFI_AUTH_OPEN: return WifiSecurity::Open;
    case WIFI_AUTH_WEP: return WifiSecurity::Wep;
    case WIFI_AUTH_WPA_PSK: return WifiSecurity::Wpa;
    case WIFI_AUTH_WPA2_PSK: return WifiSecurity::Wpa2;
    case WIFI_AUTH_WPA3_PSK: return WifiSecurity::Wpa3;
    case WIFI_AUTH_WPA2_ENTERPRISE: return WifiSecurity::Enterprise;
    default: return WifiSecurity::Unknown;
  }
}

void appendObservation(const ContactObservation &entry) {
  if (!app.sdAvailable) return;
  File file = SD.open(kObservationPath, FILE_APPEND);
  if (!file) return;
  file.printf("%lu,%s,\"%s\",%s,%d,%u,%u,",
              static_cast<unsigned long>(millis()),
              entry.protocol == ContactProtocol::Wifi ? "Wi-Fi" : "BLE",
              entry.name, entry.address, entry.rssiDbm, entry.channel,
              static_cast<unsigned>(entry.security));
  if (app.gps.hasFix && app.gps.receiverDetected) {
    file.printf("%.7f,%.7f\n", app.gps.latitude, app.gps.longitude);
  } else {
    file.println(",");
  }
  file.close();
}

void refreshSdDirectory() {
  app.sdEntryCount = 0;
  if (!app.sdAvailable) return;
  app.sdTotalBytes = SD.totalBytes();
  app.sdUsedBytes = SD.usedBytes();
  File root = SD.open("/");
  if (!root || !root.isDirectory()) return;
  File entry = root.openNextFile();
  while (entry && app.sdEntryCount < kMaxSdEntries) {
    SdEntry &out = app.sdEntries[app.sdEntryCount++];
    strlcpy(out.name, entry.name(), sizeof(out.name));
    out.isDirectory = entry.isDirectory();
    out.size = entry.isDirectory() ? 0 : entry.size();
    entry.close();
    entry = root.openNextFile();
  }
  root.close();
}

void initializeStorage() {
  app.sdAvailable = SD.begin(pins.sdCs, SPI, kSdSpiHz, "/sd", 8, false);
  if (app.sdAvailable) {
    app.sdTotalBytes = SD.totalBytes();
    app.sdUsedBytes = SD.usedBytes();
    if (!SD.exists(kObservationPath)) {
      File file = SD.open(kObservationPath, FILE_WRITE);
      if (file) {
        file.println("uptime_ms,protocol,name,address,rssi_dbm,channel,security,latitude,longitude");
        file.close();
      }
    }
    refreshSdDirectory();
  }
}

void requestPassiveScan() {
  if (!app.labAcknowledged || !app.wifiReady || wifiScanPending) return;
  if (app.sdAvailable) refreshSdDirectory();
  const int16_t result = WiFi.scanNetworks(true, true, true, kWifiDwellMs, 0);
  if (result == WIFI_SCAN_FAILED) {
    app.wifiScanFailed = true;
    app.wifiScanInProgress = false;
    return;
  }
  app.wifiScanFailed = false;
  app.wifiScanInProgress = true;
  wifiScanPending = true;
  lastScanRequest = millis();
}

void processWifiResults(int16_t count) {
  for (int16_t i = 0; i < count; ++i) {
    const String ssid = WiFi.SSID(i);
    const String bssid = WiFi.BSSIDstr(i);
    const int index = upsertContact(ContactProtocol::Wifi, ssid.c_str(), bssid.c_str(),
                                    WiFi.RSSI(i), WiFi.channel(i),
                                    securityFromAuth(WiFi.encryptionType(i)));
    if (index >= 0) appendObservation(app.contacts[index]);
  }
}

void processBleResults() {
  if (!bleScan || !app.labAcknowledged) return;
  app.bleScanInProgress = true;
  BLEScanResults *results = bleScan->start(kBleScanSeconds, false);
  if (!results) {
    app.bleScanInProgress = false;
    return;
  }
  const int count = results->getCount();
  for (int i = 0; i < count; ++i) {
    BLEAdvertisedDevice device = results->getDevice(i);
    const String address = device.getAddress().toString();
    const String name = device.getName();
    const int index = upsertContact(ContactProtocol::Ble, name.c_str(), address.c_str(),
                                    device.getRSSI(), 0, WifiSecurity::Unknown);
    if (index >= 0) appendObservation(app.contacts[index]);
  }
  bleScan->clearResults();
  app.bleScanInProgress = false;
}

void pollWifiScan() {
  if (!wifiScanPending) return;
  const int16_t result = WiFi.scanComplete();
  if (result == WIFI_SCAN_RUNNING) return;
  wifiScanPending = false;
  app.wifiScanInProgress = false;
  if (result == WIFI_SCAN_FAILED) {
    app.wifiScanFailed = true;
    WiFi.scanDelete();
    return;
  }
  app.wifiScanFailed = false;
  processWifiResults(result);
  WiFi.scanDelete();
  ++app.scanCount;
  processBleResults();
  refreshSdDirectory();
}

void loadPreferences() {
  prefs.begin("wardriver", true);
  app.honorNomap = prefs.getBool("nomap", true);
  app.labAcknowledged = prefs.getBool("labok", false);
  cueMode = prefs.getUChar("cue", 0);
  prefs.end();
  app.labCheckboxChecked = false;
}

void saveLabAcknowledgement() {
  prefs.begin("wardriver", false);
  prefs.putBool("labok", true);
  prefs.end();
  app.labAcknowledged = true;
}

void handleUiAction(UiAction action) {
  if (action == UiAction::ToggleLabCheck) {
    app.labCheckboxChecked = !app.labCheckboxChecked;
    return;
  }
  if (action == UiAction::ConfirmLabUse) {
    if (app.labCheckboxChecked) saveLabAcknowledgement();
    return;
  }
  if (!app.labAcknowledged) return;

  switch (action) {
    case UiAction::OpenWardrive:
    case UiAction::StartPassiveScan:
      requestPassiveScan();
      break;
    case UiAction::OpenSdBrowser:
      refreshSdDirectory();
      break;
    case UiAction::ExportCsv:
      if (app.sdAvailable) refreshSdDirectory();
      break;
    case UiAction::TogglePrivacyNomap:
      app.honorNomap = !app.honorNomap;
      prefs.begin("wardriver", false);
      prefs.putBool("nomap", app.honorNomap);
      prefs.end();
      break;
    case UiAction::SetCueSilent: cueMode = 0; break;
    case UiAction::SetCueBeep: cueMode = 1; break;
    case UiAction::SetCueVibration: cueMode = 2; break;
    case UiAction::SetCueBoth: cueMode = 3; break;
    default: break;
  }

  if (action == UiAction::SetCueSilent || action == UiAction::SetCueBeep ||
      action == UiAction::SetCueVibration || action == UiAction::SetCueBoth) {
    prefs.begin("wardriver", false);
    prefs.putUChar("cue", cueMode);
    prefs.end();
  }
}

void initializeRadios() {
  // Station mode with no connection; all Wi-Fi discovery requests are passive.
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  app.wifiReady = true;

  // BLE scan mode remains passive; Classic Bluetooth is not available on C5.
  BLEDevice::init("");
  bleScan = BLEDevice::getScan();
  if (bleScan) {
    bleScan->setActiveScan(false);
    bleScan->setInterval(160);
    bleScan->setWindow(80);
    app.bleAvailable = true;
  }
}

void initializeGps() {
  app.gps.receiverConfigured = true;
  gpsUart.begin(kGpsBaud, SERIAL_8N1, pins.gpsRx, pins.gpsTx);
}

void initializeSharedBusPins() {
  // Keep every active-low peripheral deselected while SPI and display start.
  pinMode(pins.tftCs, OUTPUT);
  pinMode(pins.touchCs, OUTPUT);
  pinMode(pins.sdCs, OUTPUT);
  pinMode(pins.nrfCsn, OUTPUT);
  pinMode(pins.ccCs, OUTPUT);
  digitalWrite(pins.tftCs, HIGH);
  digitalWrite(pins.touchCs, HIGH);
  digitalWrite(pins.sdCs, HIGH);
  digitalWrite(pins.nrfCsn, HIGH);
  digitalWrite(pins.ccCs, HIGH);
  pinMode(pins.nrfCe, OUTPUT);
  digitalWrite(pins.nrfCe, LOW);
}

void refreshUi() {
  if (millis() - lastUiDraw < kUiRefreshMs) return;
  lastUiDraw = millis();
  ui.draw(app);
}

void pollTouch() {
  uint16_t x = 0, y = 0;
  const bool down = readCalibratedTouch(x, y);
  if (down && !touchWasDown) {
    touchWasDown = true;
    touchDownAt = millis();
    touchDownX = x;
    touchDownY = y;
  } else if (down && touchWasDown) {
    // Keep the initial press point; touch jitter should not change the target.
  } else if (!down && touchWasDown) {
    const uint32_t held = millis() - touchDownAt;
    const UiAction action = ui.handleTouch(touchDownX, touchDownY, held, app);
    handleUiAction(action);
    touchWasDown = false;
    ui.draw(app);
    lastUiDraw = millis();
  }
}

void prunePrivacyExcluded() {
  uint8_t out = 0;
  for (uint8_t i = 0; i < app.contactCount; ++i) {
    if (privacyExcluded(app.contacts[i].name)) continue;
    if (out != i) app.contacts[out] = app.contacts[i];
    ++out;
  }
  app.contactCount = out;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);
  loadPreferences();
  initializeSharedBusPins();
  ui.begin(SPI, pins);
  pinMode(pins.touchCs, OUTPUT);
  digitalWrite(pins.touchCs, HIGH);

  if (!loadTouchCalibration()) {
    if (!calibrateTouch()) {
      // Do not silently invent calibration. Keep the device at the prompt.
      while (true) {
        ui.drawCalibrationError();
        delay(500);
      }
    }
  }

  initializeStorage();
  initializeGps();
  initializeRadios();
  ui.draw(app);
  lastUiDraw = millis();
}

void loop() {
  pollGps();
  pollWifiScan();
  pollTouch();
  if (millis() - lastLogRefresh > kLogRefreshMs) {
    lastLogRefresh = millis();
    if (app.sdAvailable) refreshSdDirectory();
  }
  refreshUi();
  delay(4);
}

