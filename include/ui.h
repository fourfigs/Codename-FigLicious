#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "app_state.h"
#include "board_pins.h"

class WardriverUI {
 public:
  bool begin(SPIClass& spi, const BoardPins& pins);
  bool draw(const AppState& state, bool force = false);
  UiAction handleTouch(uint16_t x, uint16_t y, uint32_t heldMs,
                       const AppState& state);
  void drawCalibrationTarget(uint8_t index, uint8_t total,
                             uint16_t targetX, uint16_t targetY);
  void drawCalibrationError();

  int16_t selectedContactIndex() const { return selectedContact_; }
  int16_t selectedSdEntryIndex() const { return selectedSdEntry_; }

 private:
  enum class Page : uint8_t { Home, SdBrowser, Wardrive, GpsDiagnostics };
  enum class Tab : uint8_t {
    Hunt, Radar, Finds, Security, Channel, Spectrum,
    Field, Tools, Alerts, Gps, Privacy, Transfer
  };
  enum class Filter : uint8_t { All, Wifi, Ble, Classic, Cellular, Mesh, AdsB };
  enum class Sort : uint8_t { Rssi, Recency, Sightings, Name };
  enum class CueMode : uint8_t { Silent, Beep, Vibration, Both };

  SPIClass* spi_ = nullptr;
  BoardPins pins_{};
  Page page_ = Page::Home;
  Tab tab_ = Tab::Hunt;
  Filter filter_ = Filter::All;
  Sort sort_ = Sort::Rssi;
  CueMode cueMode_ = CueMode::Silent;
  uint8_t tabStart_ = 0;
  int16_t selectedContact_ = -1;
  int16_t selectedSdEntry_ = -1;
  bool foxhunting_ = false;
  bool honorNomap_ = true;
  bool dirty_ = true;
  uint32_t lastDrawMs_ = 0;

  // ST7796S drawing primitives. The SPI object is shared with touch and SD.
  void initDisplay();
  void command(uint8_t value);
  void data(uint8_t value);
  void commandData(uint8_t command, const uint8_t* bytes, size_t count);
  void addressWindow(int16_t x, int16_t y, int16_t w, int16_t h);
  void pushPixels(const uint16_t* pixels, size_t count);
  void fillScreen(uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  void drawCircle(int16_t cx, int16_t cy, int16_t radius, uint16_t color);
  void fillCircle(int16_t cx, int16_t cy, int16_t radius, uint16_t color);
  void text(int16_t x, int16_t y, const char* value, uint16_t fg,
            uint16_t bg, uint8_t scale = 1, int16_t maxWidth = 460);
  void textCenter(int16_t x, int16_t y, int16_t width, const char* value,
                  uint16_t fg, uint16_t bg, uint8_t scale = 1);

  void drawChrome(const AppState& state, const char* title);
  void drawDock();
  void drawHome(const AppState& state);
  void drawLabGate(const AppState& state);
  void drawSdBrowser(const AppState& state);
  void drawGps(const AppState& state, bool compact);
  void drawWardrive(const AppState& state);
  void drawTabs();
  void drawHunt(const AppState& state);
  void drawRadar(const AppState& state);
  void drawFinds(const AppState& state);
  void drawSecurity(const AppState& state);
  void drawChannels(const AppState& state, bool spectrum);
  void drawField(const AppState& state);
  void drawTools(const AppState& state);
  void drawAlerts(const AppState& state);
  void drawPrivacy();
  void drawTransfer(const AppState& state);
  void drawFoxhunt(const AppState& state);
  void drawContactRow(const ContactObservation& contact, int16_t y,
                      bool selected = false);

  uint8_t sortedContacts(const AppState& state, uint8_t* indices,
                         bool wifiOnly = false) const;
  static const char* tabName(Tab tab);
  static const char* securityName(WifiSecurity security);
  static const char* filterName(Filter filter);
  static const char* sortName(Sort sort);
  static const char* glyph(char c);
  static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b);
  static const char* ageText(uint32_t thenMs, char* out, size_t outSize);
  static const char* bytesText(uint64_t bytes, char* out, size_t outSize);
};

