#pragma once

#include <stdint.h>

// Firmware-wide data contract. Values are populated only by hardware-backed
// services; zeroed/unavailable fields mean there is no current observation.
constexpr uint8_t kMaxContacts = 40;
constexpr uint8_t kMaxSdEntries = 16;

enum class ContactProtocol : uint8_t { Wifi = 0, Ble = 1 };
enum class WifiSecurity : uint8_t { Unknown = 0, Open, Wep, Wpa, Wpa2, Wpa3, Enterprise };

struct ContactObservation {
  ContactProtocol protocol = ContactProtocol::Wifi;
  char name[33] = {};
  char address[18] = {};
  int8_t rssiDbm = -127;
  uint8_t channel = 0;
  WifiSecurity security = WifiSecurity::Unknown;
  uint16_t sightings = 0;
  uint32_t lastSeenMs = 0;
  bool hasLocation = false;
  double latitude = 0.0;
  double longitude = 0.0;
};

struct GpsState {
  bool receiverConfigured = false;
  bool receiverDetected = false;
  bool hasFix = false;
  double latitude = 0.0;
  double longitude = 0.0;
  float speedKmh = 0.0f;
  float courseDegrees = 0.0f;
  float hdop = 0.0f;
  uint8_t satellites = 0;
  uint32_t lastSentenceMs = 0;
};

struct SdEntry {
  char name[64] = {};
  uint32_t size = 0;
  bool isDirectory = false;
};

struct AppState {
  GpsState gps;
  bool sdAvailable = false;
  uint64_t sdTotalBytes = 0;
  uint64_t sdUsedBytes = 0;
  uint8_t sdEntryCount = 0;
  SdEntry sdEntries[kMaxSdEntries];
  bool wifiReady = false;
  bool wifiScanInProgress = false;
  bool wifiScanFailed = false;
  bool bleAvailable = false;
  bool bleScanInProgress = false;
  bool honorNomap = true;
  bool labAcknowledged = false;
  bool labCheckboxChecked = false;
  uint16_t scanCount = 0;
  uint8_t contactCount = 0;
  ContactObservation contacts[kMaxContacts];
  bool touchCalibrated = false;
  bool beepAvailable = false;
  bool vibrationAvailable = false;
};

enum class UiAction : uint8_t {
  None = 0,
  OpenHome,
  OpenWardrive,
  OpenSdBrowser,
  OpenSdFile,
  StartPassiveScan,
  OpenGpsDiagnostics,
  ExportCsv,
  TogglePrivacyNomap,
  ToggleLabCheck,
  ConfirmLabUse,
  SetCueSilent,
  SetCueBeep,
  SetCueVibration,
  SetCueBoth
};

