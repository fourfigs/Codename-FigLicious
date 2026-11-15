#include "ui.h"

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

namespace {
constexpr int16_t kWidth = 480;
constexpr int16_t kHeight = 320;
constexpr uint16_t C_BG = 0x0841;
constexpr uint16_t C_PANEL = 0x10A4;
constexpr uint16_t C_PANEL_DARK = 0x08A2;
constexpr uint16_t C_LINE = 0x1B8D;
constexpr uint16_t C_CYAN = 0x2F7D;
constexpr uint16_t C_GOLD = 0xFD20;
constexpr uint16_t C_PINK = 0xF81F;
constexpr uint16_t C_GREEN = 0x7F6D;
constexpr uint16_t C_AMBER = 0xFCA0;
constexpr uint16_t C_RED = 0xF986;
constexpr uint16_t C_TEXT = 0xEF7D;
constexpr uint16_t C_MUTED = 0x9D13;
constexpr uint16_t C_DARK = 0x0000;
constexpr uint8_t kTabCount = 12;
constexpr uint8_t kVisibleTabs = 6;
constexpr int16_t kTabX = 32;
constexpr int16_t kTabWidth = 69;
constexpr int16_t kTabY = 47;
constexpr int16_t kTabH = 34;
constexpr int16_t kDockY = 283;

const char* const kTabLabels[kTabCount] = {
    "HUNT", "RADAR", "FINDS", "SEC", "CHANNEL", "SPECTRUM",
    "FIELD", "TOOLS", "ALERTS", "GPS", "PRIVACY", "XFER"};

uint8_t authRank(WifiSecurity sec) { return static_cast<uint8_t>(sec); }

void safeCopy(char* dst, size_t size, const char* src) {
  if (size == 0) return;
  if (src == nullptr) src = "";
  strncpy(dst, src, size - 1);
  dst[size - 1] = '\0';
}
}  // namespace

uint16_t WardriverUI::rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

bool WardriverUI::begin(SPIClass& spi, const BoardPins& pins) {
  spi_ = &spi;
  pins_ = pins;

  // Deselect every active-low device before sharing the bus with the display.
  pinMode(pins_.tftCs, OUTPUT);
  digitalWrite(pins_.tftCs, HIGH);
  pinMode(pins_.touchCs, OUTPUT);
  digitalWrite(pins_.touchCs, HIGH);
  pinMode(pins_.sdCs, OUTPUT);
  digitalWrite(pins_.sdCs, HIGH);
  pinMode(pins_.nrfCsn, OUTPUT);
  digitalWrite(pins_.nrfCsn, HIGH);
  pinMode(pins_.ccCs, OUTPUT);
  digitalWrite(pins_.ccCs, HIGH);
  pinMode(pins_.nrfCe, OUTPUT);
  digitalWrite(pins_.nrfCe, LOW);
  pinMode(pins_.tftDc, OUTPUT);
  digitalWrite(pins_.tftDc, HIGH);

  // The shared bus is initialized once here; touch, SD, and radios can reuse it.
  spi_->begin(pins_.spiSck, pins_.spiMiso, pins_.spiMosi, -1);
  initDisplay();
  dirty_ = true;
  return true;
}

void WardriverUI::command(uint8_t value) {
  if (!spi_) return;
  spi_->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(pins_.tftCs, LOW);
  digitalWrite(pins_.tftDc, LOW);
  spi_->transfer(value);
  digitalWrite(pins_.tftCs, HIGH);
  spi_->endTransaction();
}

void WardriverUI::data(uint8_t value) {
  if (!spi_) return;
  spi_->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(pins_.tftCs, LOW);
  digitalWrite(pins_.tftDc, HIGH);
  spi_->transfer(value);
  digitalWrite(pins_.tftCs, HIGH);
  spi_->endTransaction();
}

void WardriverUI::commandData(uint8_t cmd, const uint8_t* bytes, size_t count) {
  if (!spi_) return;
  spi_->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(pins_.tftCs, LOW);
  digitalWrite(pins_.tftDc, LOW);
  spi_->transfer(cmd);
  if (count) {
    digitalWrite(pins_.tftDc, HIGH);
    for (size_t i = 0; i < count; ++i) spi_->transfer(bytes[i]);
  }
  digitalWrite(pins_.tftCs, HIGH);
  spi_->endTransaction();
}

void WardriverUI::initDisplay() {
  delay(10);
  command(0x01);  // Software reset; hardware RESET is tied high.
  delay(120);
  command(0x11);  // Sleep out.
  delay(120);
  const uint8_t madctl = 0x28;  // Landscape (MV) with BGR order.
  commandData(0x36, &madctl, 1);
  const uint8_t pixelFormat = 0x55;  // RGB565.
  commandData(0x3A, &pixelFormat, 1);
  const uint8_t porch[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
  commandData(0xB2, porch, sizeof(porch));
  const uint8_t gate = 0x35;
  commandData(0xB7, &gate, 1);
  const uint8_t vcom[] = {0x1F};
  commandData(0xBB, vcom, sizeof(vcom));
  const uint8_t lcm = 0x2C;
  commandData(0xC0, &lcm, 1);
  const uint8_t power[] = {0x01, 0xC3};
  commandData(0xC2, power, sizeof(power));
  const uint8_t vcom2[] = {0x20};
  commandData(0xC5, vcom2, sizeof(vcom2));
  const uint8_t positive[] = {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x05,
                              0x31, 0x33, 0x48, 0x17, 0x14, 0x15,
                              0x31, 0x34};
  const uint8_t negative[] = {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x15,
                              0x31, 0x33, 0x48, 0x17, 0x14, 0x15,
                              0x31, 0x34};
  commandData(0xE0, positive, sizeof(positive));
  commandData(0xE1, negative, sizeof(negative));
  const uint8_t invert = 0x00;
  commandData(0x21, &invert, 0);
  command(0x29);  // Display on.
  delay(20);
  fillScreen(C_BG);
}

void WardriverUI::addressWindow(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > kWidth || y + h > kHeight) return;
  const uint16_t x1 = static_cast<uint16_t>(x + w - 1);
  const uint16_t y1 = static_cast<uint16_t>(y + h - 1);
  const uint8_t col[] = {static_cast<uint8_t>(x >> 8), static_cast<uint8_t>(x),
                         static_cast<uint8_t>(x1 >> 8), static_cast<uint8_t>(x1)};
  const uint8_t row[] = {static_cast<uint8_t>(y >> 8), static_cast<uint8_t>(y),
                         static_cast<uint8_t>(y1 >> 8), static_cast<uint8_t>(y1)};
  commandData(0x2A, col, sizeof(col));
  commandData(0x2B, row, sizeof(row));
  command(0x2C);
}

void WardriverUI::pushPixels(const uint16_t* pixels, size_t count) {
  if (!spi_ || !pixels || count == 0) return;
  spi_->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(pins_.tftCs, LOW);
  digitalWrite(pins_.tftDc, HIGH);
  for (size_t i = 0; i < count; ++i) spi_->transfer16(pixels[i]);
  digitalWrite(pins_.tftCs, HIGH);
  spi_->endTransaction();
}

void WardriverUI::fillScreen(uint16_t color) { fillRect(0, 0, kWidth, kHeight, color); }

void WardriverUI::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > kWidth) w = kWidth - x;
  if (y + h > kHeight) h = kHeight - y;
  if (w <= 0 || h <= 0 || !spi_) return;
  addressWindow(x, y, w, h);
  spi_->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(pins_.tftCs, LOW);
  digitalWrite(pins_.tftDc, HIGH);
  const size_t n = static_cast<size_t>(w) * h;
  for (size_t i = 0; i < n; ++i) spi_->transfer16(color);
  digitalWrite(pins_.tftCs, HIGH);
  spi_->endTransaction();
}

void WardriverUI::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  fillRect(x, y, w, 1, color);
  fillRect(x, y + h - 1, w, 1, color);
  fillRect(x, y, 1, h, color);
  fillRect(x + w - 1, y, 1, h, color);
}

void WardriverUI::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  const int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  const int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy;
  while (true) {
    fillRect(x0, y0, 1, 1, color);
    if (x0 == x1 && y0 == y1) break;
    const int16_t e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void WardriverUI::drawCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  int16_t x = r, y = 0, err = 0;
  while (x >= y) {
    fillRect(cx + x, cy + y, 1, 1, color); fillRect(cx + y, cy + x, 1, 1, color);
    fillRect(cx - y, cy + x, 1, 1, color); fillRect(cx - x, cy + y, 1, 1, color);
    fillRect(cx - x, cy - y, 1, 1, color); fillRect(cx - y, cy - x, 1, 1, color);
    fillRect(cx + y, cy - x, 1, 1, color); fillRect(cx + x, cy - y, 1, 1, color);
    if (err <= 0) { ++y; err += 2 * y + 1; }
    if (err > 0) { --x; err -= 2 * x + 1; }
  }
}

void WardriverUI::fillCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  for (int16_t y = -r; y <= r; ++y) {
    const int16_t dx = static_cast<int16_t>(sqrtf(static_cast<float>(r * r - y * y)));
    fillRect(cx - dx, cy + y, dx * 2 + 1, 1, color);
  }
}

const char* WardriverUI::glyph(char c) {
  static const char blank[5] = {0, 0, 0, 0, 0};
  // Five vertical columns, least-significant bit at the top.
  static const char letters[26][5] = {
      {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
      {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
      {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
      {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
      {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
      {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
      {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
      {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
      {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
      {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
      {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
      {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
      {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}};
  static const char digits[10][5] = {
      {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
      {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
      {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
      {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
      {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}};
  static const char dash[5] = {0x08,0x08,0x08,0x08,0x08};
  static const char period[5] = {0x00,0x60,0x60,0x00,0x00};
  static const char colon[5] = {0x00,0x36,0x36,0x00,0x00};
  static const char slash[5] = {0x20,0x10,0x08,0x04,0x02};
  static const char plus[5] = {0x08,0x08,0x3E,0x08,0x08};
  static const char question[5] = {0x02,0x01,0x51,0x09,0x06};
  static const char percent[5] = {0x63,0x13,0x08,0x64,0x63};
  static const char underscore[5] = {0x40,0x40,0x40,0x40,0x40};
  static const char star[5] = {0x14,0x08,0x3E,0x08,0x14};
  static const char left[5] = {0x08,0x1C,0x2A,0x08,0x08};
  static const char right[5] = {0x08,0x08,0x2A,0x1C,0x08};
  static const char at[5] = {0x3E,0x41,0x5D,0x55,0x1E};
  if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
  if (c >= '0' && c <= '9') return digits[c - '0'];
  switch (c) {
    case '-': return dash; case '.': return period; case ':': return colon;
    case '/': return slash; case '+': return plus; case '?': return question;
    case '%': return percent; case '_': return underscore; case '*': return star;
    case '<': return left; case '>': return right; case '@': return at;
    default: return blank;
  }
}

void WardriverUI::text(int16_t x, int16_t y, const char* value, uint16_t fg,
                       uint16_t bg, uint8_t scale, int16_t maxWidth) {
  if (!value || scale == 0 || maxWidth <= 0) return;
  const int16_t charsFit = maxWidth / (6 * scale);
  const size_t chars = strlen(value) < static_cast<size_t>(charsFit)
                           ? strlen(value) : static_cast<size_t>(charsFit);
  const int16_t width = static_cast<int16_t>(chars * 6 * scale);
  if (width <= 0) return;
  uint16_t row[480];
  const int16_t height = 7 * scale;
  for (int16_t ry = 0; ry < height; ++ry) {
    for (int16_t i = 0; i < width; ++i) row[i] = bg;
    const int16_t gy = ry / scale;
    for (size_t ci = 0; ci < chars; ++ci) {
      const char* g = glyph(value[ci]);
      const int16_t gx = static_cast<int16_t>(ci * 6 * scale);
      for (int16_t col = 0; col < 5; ++col) {
        if ((static_cast<uint8_t>(g[col]) >> gy) & 1U) {
          for (uint8_t sx = 0; sx < scale; ++sx)
            for (uint8_t sy = 0; sy < scale; ++sy)
              row[gx + col * scale + sx] = fg;
        }
      }
    }
    addressWindow(x, y + ry, width, 1);
    pushPixels(row, width);
  }
}

void WardriverUI::textCenter(int16_t x, int16_t y, int16_t width, const char* value,
                             uint16_t fg, uint16_t bg, uint8_t scale) {
  const int16_t textWidth = static_cast<int16_t>(strlen(value) * 6 * scale);
  text(x + (width - textWidth) / 2, y, value, fg, bg, scale, width);
}

const char* WardriverUI::tabName(Tab tab) {
  const uint8_t i = static_cast<uint8_t>(tab);
  return i < kTabCount ? kTabLabels[i] : "HUNT";
}

const char* WardriverUI::securityName(WifiSecurity security) {
  switch (security) {
    case WifiSecurity::Open: return "OPEN";
    case WifiSecurity::Wep: return "WEP";
    case WifiSecurity::Wpa: return "WPA";
    case WifiSecurity::Wpa2: return "WPA2";
    case WifiSecurity::Wpa3: return "WPA3";
    case WifiSecurity::Enterprise: return "ENTERPRISE";
    default: return "UNKNOWN";
  }
}

const char* WardriverUI::filterName(Filter filter) {
  static const char* names[] = {"ALL", "WIFI", "BLE", "CLASSIC", "CELL", "MESH", "ADS-B"};
  const uint8_t i = static_cast<uint8_t>(filter);
  return i < 7 ? names[i] : "ALL";
}

const char* WardriverUI::sortName(Sort sort) {
  static const char* names[] = {"RSSI", "RECENT", "SEEN", "NAME"};
  const uint8_t i = static_cast<uint8_t>(sort);
  return i < 4 ? names[i] : "RSSI";
}

const char* WardriverUI::ageText(uint32_t thenMs, char* out, size_t outSize) {
  const uint32_t seconds = (millis() - thenMs) / 1000U;
  if (seconds < 2) snprintf(out, outSize, "NOW");
  else if (seconds < 60) snprintf(out, outSize, "%luS", static_cast<unsigned long>(seconds));
  else snprintf(out, outSize, "%luM", static_cast<unsigned long>(seconds / 60U));
  return out;
}

const char* WardriverUI::bytesText(uint64_t bytes, char* out, size_t outSize) {
  if (bytes >= 1024ULL * 1024ULL) snprintf(out, outSize, "%llu MB", bytes / (1024ULL * 1024ULL));
  else if (bytes >= 1024ULL) snprintf(out, outSize, "%llu KB", bytes / 1024ULL);
  else snprintf(out, outSize, "%llu B", bytes);
  return out;
}

void WardriverUI::drawChrome(const AppState& state, const char* title) {
  fillRect(0, 0, kWidth, 43, C_PANEL_DARK);
  fillRect(0, 42, kWidth, 1, C_CYAN);
  text(12, 8, "GNOME HOME", C_TEXT, C_PANEL_DARK, 2, 190);
  text(12, 29, title, C_GOLD, C_PANEL_DARK, 1, 210);
  int16_t x = 232;
  const uint16_t gpsColor = state.gps.hasFix ? C_GREEN : C_AMBER;
  const char* gpsLabel = state.gps.hasFix ? "GPS FIX" : (state.gps.receiverDetected ? "GPS SEARCH" : "NO GPS");
  drawRect(x, 9, 76, 23, C_LINE);
  textCenter(x + 2, 17, 72, gpsLabel, gpsColor, C_PANEL_DARK, 1);
  x += 82;
  drawRect(x, 9, 70, 23, state.wifiReady ? C_LINE : C_RED);
  textCenter(x + 1, 17, 68, state.wifiReady ? "WIFI RX" : "WIFI OFF", state.wifiReady ? C_CYAN : C_MUTED, C_PANEL_DARK, 1);
  x += 76;
  drawRect(x, 9, 65, 23, state.sdAvailable ? C_LINE : C_RED);
  textCenter(x + 1, 17, 63, state.sdAvailable ? "SD READY" : "NO SD", state.sdAvailable ? C_GREEN : C_MUTED, C_PANEL_DARK, 1);
  x += 71;
  drawRect(x, 9, 33, 23, state.touchCalibrated ? C_LINE : C_AMBER);
  textCenter(x + 1, 17, 31, state.touchCalibrated ? "CAL" : "!", state.touchCalibrated ? C_GREEN : C_AMBER, C_PANEL_DARK, 1);
}

void WardriverUI::drawDock() {
  fillRect(0, kDockY, kWidth, kHeight - kDockY, C_PANEL_DARK);
  fillRect(0, kDockY, kWidth, 1, C_LINE);
  const bool home = page_ == Page::Home;
  const bool ride = page_ == Page::Wardrive;
  const bool sd = page_ == Page::SdBrowser;
  const char* labels[] = {"HOME", "WARD RIDE", "SD CARD"};
  const int16_t left[] = {4, 164, 324};
  for (uint8_t i = 0; i < 3; ++i) {
    const bool active = (i == 0 && home) || (i == 1 && ride) || (i == 2 && sd);
    const uint16_t border = active ? C_GOLD : C_LINE;
    const uint16_t fg = active ? C_TEXT : C_MUTED;
    fillRect(left[i], 289, 152, 25, active ? 0x2126 : C_PANEL_DARK);
    drawRect(left[i], 289, 152, 25, border);
    textCenter(left[i] + 2, 298, 148, labels[i], fg, active ? 0x2126 : C_PANEL_DARK, 1);
  }
}

void WardriverUI::drawGps(const AppState& state, bool compact) {
  if (!state.gps.receiverConfigured) {
    text(24, compact ? 105 : 72, "GPS RECEIVER NOT CONFIGURED", C_AMBER, C_PANEL, 2, 430);
    text(24, compact ? 130 : 98, "NO LIVE POSITION IS AVAILABLE", C_MUTED, C_PANEL, 1, 430);
    return;
  }
  if (!state.gps.receiverDetected) {
    text(24, compact ? 105 : 72, "GPS RECEIVER NOT DETECTED", C_AMBER, C_PANEL, 2, 430);
    text(24, compact ? 130 : 98, "CHECK UART TX TO D7 / GPIO12 RX", C_MUTED, C_PANEL, 1, 430);
    return;
  }
  if (!state.gps.hasFix) {
    text(24, compact ? 105 : 72, "NO FIX - SEARCHING", C_AMBER, C_PANEL, 2, 430);
    char line[48];
    snprintf(line, sizeof(line), "SATELLITES: %u", state.gps.satellites);
    text(24, compact ? 132 : 101, line, C_TEXT, C_PANEL, 1, 430);
    return;
  }
  char line[64];
  snprintf(line, sizeof(line), "%.5f N", fabs(state.gps.latitude));
  if (state.gps.latitude < 0) line[strlen(line) - 1] = 'S';
  text(24, compact ? 101 : 72, line, C_TEXT, C_PANEL, 2, 205);
  snprintf(line, sizeof(line), "%.5f E", fabs(state.gps.longitude));
  if (state.gps.longitude < 0) line[strlen(line) - 1] = 'W';
  text(245, compact ? 101 : 72, line, C_TEXT, C_PANEL, 2, 205);
  snprintf(line, sizeof(line), "SPEED %.1f KM/H", state.gps.speedKmh);
  text(24, compact ? 139 : 108, line, C_CYAN, C_PANEL, 1, 170);
  snprintf(line, sizeof(line), "COURSE %.0f DEG", state.gps.courseDegrees);
  text(204, compact ? 139 : 108, line, C_CYAN, C_PANEL, 1, 150);
  snprintf(line, sizeof(line), "SAT %u  HDOP %.1f", state.gps.satellites, state.gps.hdop);
  text(365, compact ? 139 : 108, line, C_CYAN, C_PANEL, 1, 105);
}

void WardriverUI::drawHome(const AppState& state) {
  drawChrome(state, "FIELD DASHBOARD");
  fillRect(12, 52, 456, 126, C_PANEL);
  drawRect(12, 52, 456, 126, C_GOLD);
  text(24, 61, "MISS DIRECTION", C_GOLD, C_PANEL, 1, 250);
  drawGps(state, true);
  // A restrained, hand-drawn radio compass motif; it contains no live bearing.
  drawCircle(430, 112, 34, C_LINE);
  drawCircle(430, 112, 23, C_LINE);
  drawLine(430, 75, 430, 149, C_LINE);
  drawLine(393, 112, 467, 112, C_LINE);
  drawLine(411, 130, 449, 94, C_GOLD);
  textCenter(388, 151, 84, "GPS", C_MUTED, C_PANEL, 1);

  fillRect(12, 190, 222, 82, 0x20E5);
  drawRect(12, 190, 222, 82, C_PINK);
  text(24, 203, "LADY DUALBANDIT", C_PINK, 0x20E5, 1, 198);
  text(24, 226, "WARD RIDE / LIVE HUNT", C_TEXT, 0x20E5, 1, 198);
  text(24, 249, "TAP TO OPEN >", C_CYAN, 0x20E5, 1, 198);

  fillRect(246, 190, 222, 82, 0x39A4);
  drawRect(246, 190, 222, 82, C_GOLD);
  text(258, 203, "CACHE CASSIDY", C_GOLD, 0x39A4, 1, 198);
  text(258, 226, "LOCAL SD CARD BROWSER", C_TEXT, 0x39A4, 1, 198);
  text(258, 249, "TAP TO BROWSE >", C_CYAN, 0x39A4, 1, 198);
  text(18, 274, "RECEIVE-ONLY OBSERVATIONS  |  NO SAMPLE CONTACTS", C_MUTED, C_BG, 1, 455);
  drawDock();
}

void WardriverUI::drawLabGate(const AppState& state) {
  fillScreen(C_BG);
  fillRect(0, 0, kWidth, 48, C_PANEL_DARK);
  text(18, 12, "GNOME HOME / LAB USE", C_GOLD, C_PANEL_DARK, 2, 440);
  fillRect(18, 69, 444, 164, C_PANEL);
  drawRect(18, 69, 444, 164, C_CYAN);
  text(34, 88, "DESIGNED FOR LAB USE ONLY.", C_GOLD, C_PANEL, 1, 410);
  text(34, 115, "WI-FI/BLE COLLECTION IN THIS BUILD", C_TEXT, C_PANEL, 1, 410);
  text(34, 134, "IS RECEIVE-ONLY.", C_TEXT, C_PANEL, 1, 410);
  fillRect(30, 160, 420, 47, state.labCheckboxChecked ? 0x124C : C_PANEL_DARK);
  drawRect(30, 160, 420, 47, state.labCheckboxChecked ? C_GREEN : C_LINE);
  drawRect(42, 174, 18, 18, state.labCheckboxChecked ? C_GREEN : C_MUTED);
  if (state.labCheckboxChecked) {
    drawLine(45, 183, 50, 188, C_TEXT);
    drawLine(50, 188, 58, 177, C_TEXT);
  }
  text(68, 170, "I CONFIRM I WILL USE THIS ONLY IN A", C_TEXT, state.labCheckboxChecked ? 0x124C : C_PANEL_DARK, 1, 365);
  text(68, 187, "LAB OR ON SYSTEMS I'M AUTHORIZED TO ASSESS.", C_TEXT, state.labCheckboxChecked ? 0x124C : C_PANEL_DARK, 1, 365);
  const uint16_t confirmBg = state.labCheckboxChecked ? 0x3126 : C_PANEL_DARK;
  fillRect(130, 245, 220, 42, confirmBg);
  drawRect(130, 245, 220, 42, state.labCheckboxChecked ? C_PINK : C_LINE);
  textCenter(132, 262, 216, "CONFIRM AND CONTINUE", state.labCheckboxChecked ? C_TEXT : C_MUTED, confirmBg, 1);
  if (!state.labCheckboxChecked)
    textCenter(18, 296, 444, "CHECK THE BOX TO ENABLE CONTINUE", C_MUTED, C_BG, 1);
}

void WardriverUI::drawSdBrowser(const AppState& state) {
  drawChrome(state, "CACHE CASSIDY / SD");
  fillRect(12, 52, 456, 221, C_PANEL);
  drawRect(12, 52, 456, 221, C_GOLD);
  text(24, 62, "SD CARD CONTENTS", C_GOLD, C_PANEL, 1, 260);
  if (!state.sdAvailable) {
    text(24, 104, "NO SD CARD MOUNTED", C_AMBER, C_PANEL, 2, 430);
    text(24, 137, "INSERT A CARD AND RESTART OR RETRY", C_MUTED, C_PANEL, 1, 430);
  } else {
    char storage[64], total[32], used[32];
    bytesText(state.sdTotalBytes, total, sizeof(total));
    bytesText(state.sdUsedBytes, used, sizeof(used));
    snprintf(storage, sizeof(storage), "USED %s / %s", used, total);
    text(24, 83, storage, C_TEXT, C_PANEL, 1, 430);
    const uint32_t percent = state.sdTotalBytes == 0 ? 0 :
        static_cast<uint32_t>((state.sdUsedBytes * 100ULL) / state.sdTotalBytes);
    fillRect(24, 101, 420, 8, C_PANEL_DARK);
    const int16_t bar = static_cast<int16_t>((420UL * (percent > 100 ? 100 : percent)) / 100UL);
    fillRect(24, 101, bar, 8, C_CYAN);
    if (state.sdEntryCount == 0) {
      text(24, 128, "NO DIRECTORY ENTRIES REPORTED", C_MUTED, C_PANEL, 1, 420);
    } else {
      text(24, 116, "NAME", C_MUTED, C_PANEL, 1, 280);
      text(350, 116, "SIZE", C_MUTED, C_PANEL, 1, 100);
      const uint8_t count = state.sdEntryCount > kMaxSdEntries ? kMaxSdEntries : state.sdEntryCount;
      for (uint8_t i = 0; i < count && i < 6; ++i) {
        const int16_t y = 133 + i * 22;
        const uint16_t bg = (i == selectedSdEntry_) ? 0x2126 : C_PANEL_DARK;
        fillRect(20, y, 436, 20, bg);
        const SdEntry& entry = state.sdEntries[i];
        text(27, y + 6, entry.isDirectory ? ">" : "-", entry.isDirectory ? C_GOLD : C_CYAN, bg, 1, 12);
        text(43, y + 6, entry.name, C_TEXT, bg, 1, 300);
        char size[24];
        if (entry.isDirectory) safeCopy(size, sizeof(size), "DIR");
        else bytesText(entry.size, size, sizeof(size));
        text(354, y + 6, size, C_MUTED, bg, 1, 95);
      }
    }
  }
  drawDock();
}

void WardriverUI::drawTabs() {
  fillRect(0, kTabY - 2, kWidth, kTabH + 5, C_BG);
  fillRect(2, kTabY, 27, kTabH, C_PANEL_DARK);
  drawRect(2, kTabY, 27, kTabH, C_LINE);
  textCenter(3, kTabY + 13, 25, "<", C_CYAN, C_PANEL_DARK, 1);
  fillRect(451, kTabY, 27, kTabH, C_PANEL_DARK);
  drawRect(451, kTabY, 27, kTabH, C_LINE);
  textCenter(452, kTabY + 13, 25, ">", C_CYAN, C_PANEL_DARK, 1);
  for (uint8_t slot = 0; slot < kVisibleTabs; ++slot) {
    const uint8_t idx = tabStart_ + slot;
    if (idx >= kTabCount) break;
    const int16_t x = kTabX + slot * kTabWidth;
    const bool active = idx == static_cast<uint8_t>(tab_);
    const uint16_t bg = active ? 0x3126 : C_PANEL_DARK;
    const uint16_t border = active ? C_PINK : C_LINE;
    fillRect(x, kTabY, kTabWidth - 3, kTabH, bg);
    drawRect(x, kTabY, kTabWidth - 3, kTabH, border);
    textCenter(x + 1, kTabY + 13, kTabWidth - 5, kTabLabels[idx], active ? C_TEXT : C_MUTED, bg, 1);
  }
}

void WardriverUI::drawWardrive(const AppState& state) {
  drawChrome(state, "LADY DUALBANDIT / WARD RIDE");
  drawTabs();
  fillRect(10, 86, 460, 190, C_PANEL);
  drawRect(10, 86, 460, 190, C_LINE);
  if (foxhunting_) drawFoxhunt(state);
  else {
    switch (tab_) {
      case Tab::Hunt: drawHunt(state); break;
      case Tab::Radar: drawRadar(state); break;
      case Tab::Finds: drawFinds(state); break;
      case Tab::Security: drawSecurity(state); break;
      case Tab::Channel: drawChannels(state, false); break;
      case Tab::Spectrum: drawChannels(state, true); break;
      case Tab::Field: drawField(state); break;
      case Tab::Tools: drawTools(state); break;
      case Tab::Alerts: drawAlerts(state); break;
      case Tab::Gps: drawGps(state, false); break;
      case Tab::Privacy: drawPrivacy(); break;
      case Tab::Transfer: drawTransfer(state); break;
    }
  }
  drawDock();
}

uint8_t WardriverUI::sortedContacts(const AppState& state, uint8_t* indices,
                                    bool wifiOnly) const {
  uint8_t n = 0;
  const uint8_t count = state.contactCount > kMaxContacts ? kMaxContacts : state.contactCount;
  for (uint8_t i = 0; i < count; ++i) {
    const ContactObservation& c = state.contacts[i];
    if (wifiOnly && c.protocol != ContactProtocol::Wifi) continue;
    bool include = filter_ == Filter::All ||
                   (filter_ == Filter::Wifi && c.protocol == ContactProtocol::Wifi) ||
                   (filter_ == Filter::Ble && c.protocol == ContactProtocol::Ble);
    if (wifiOnly) include = c.protocol == ContactProtocol::Wifi;
    if (include) indices[n++] = i;
  }
  for (uint8_t i = 1; i < n; ++i) {
    uint8_t key = indices[i], j = i;
    while (j > 0) {
      const ContactObservation& a = state.contacts[indices[j - 1]];
      const ContactObservation& b = state.contacts[key];
      bool before = false;
      if (sort_ == Sort::Rssi) before = a.rssiDbm < b.rssiDbm;
      else if (sort_ == Sort::Recency) before = static_cast<int32_t>(a.lastSeenMs - b.lastSeenMs) < 0;
      else if (sort_ == Sort::Sightings) before = a.sightings < b.sightings;
      else before = strcasecmp(a.name, b.name) > 0;
      if (!before) break;
      indices[j] = indices[j - 1];
      --j;
    }
    indices[j] = key;
  }
  return n;
}

void WardriverUI::drawContactRow(const ContactObservation& contact, int16_t y, bool selected) {
  const uint16_t bg = selected ? 0x2126 : C_PANEL_DARK;
  fillRect(18, y, 444, 34, bg);
  const uint16_t marker = contact.protocol == ContactProtocol::Wifi ? C_CYAN : C_PINK;
  fillCircle(29, y + 17, 4, marker);
  text(40, y + 5, contact.name[0] ? contact.name : "UNNAMED", C_TEXT, bg, 1, 150);
  char meta[54], age[12];
  if (contact.protocol == ContactProtocol::Wifi)
    snprintf(meta, sizeof(meta), "WIFI CH%u %s", contact.channel, securityName(contact.security));
  else snprintf(meta, sizeof(meta), "BLE %s", contact.address);
  text(40, y + 19, meta, C_MUTED, bg, 1, 180);
  char rssi[16];
  snprintf(rssi, sizeof(rssi), "%d DBM", contact.rssiDbm);
  text(237, y + 6, rssi, C_CYAN, bg, 1, 70);
  snprintf(meta, sizeof(meta), "X%u", contact.sightings);
  text(310, y + 6, meta, C_TEXT, bg, 1, 42);
  text(354, y + 6, ageText(contact.lastSeenMs, age, sizeof(age)), C_MUTED, bg, 1, 45);
  const int16_t barW = contact.rssiDbm <= -100 ? 0 :
      contact.rssiDbm >= -30 ? 52 : (contact.rssiDbm + 100) * 52 / 70;
  fillRect(405, y + 13, 52, 7, C_BG);
  fillRect(405, y + 13, barW, 7, contact.rssiDbm > -60 ? C_GREEN : (contact.rssiDbm > -78 ? C_GOLD : C_MUTED));
}

void WardriverUI::drawHunt(const AppState& state) {
  text(20, 91, "LIVE CONTACTS", C_GOLD, C_PANEL, 1, 170);
  char countLine[30];
  snprintf(countLine, sizeof(countLine), "SEEN: %u", state.contactCount);
  text(195, 91, countLine, C_MUTED, C_PANEL, 1, 90);
  const int16_t scanX = 365;
  fillRect(scanX, 88, 91, 24, state.wifiScanInProgress ? 0x3126 : 0x10C6);
  drawRect(scanX, 88, 91, 24, state.wifiScanInProgress ? C_PINK : C_CYAN);
  textCenter(scanX + 1, 97, 89, state.wifiScanInProgress ? "SCANNING" : "PASSIVE SCAN", C_TEXT, state.wifiScanInProgress ? 0x3126 : 0x10C6, 1);

  const char* filters[] = {"ALL", "WIFI", "BLE", "CLASSIC", "CELL", "MESH", "ADS-B"};
  for (uint8_t i = 0; i < 7; ++i) {
    const int16_t x = 17 + i * 63;
    const bool active = static_cast<uint8_t>(filter_) == i;
    const uint16_t bg = active ? 0x124C : C_PANEL_DARK;
    fillRect(x, 116, 60, 21, bg);
    drawRect(x, 116, 60, 21, active ? C_CYAN : C_LINE);
    textCenter(x, 123, 60, filters[i], active ? C_TEXT : C_MUTED, bg, 1);
  }
  const char* sorts[] = {"RSSI", "RECENT", "SEEN", "NAME"};
  for (uint8_t i = 0; i < 4; ++i) {
    const int16_t x = 18 + i * 111;
    const bool active = static_cast<uint8_t>(sort_) == i;
    const uint16_t bg = active ? 0x3126 : C_PANEL;
    fillRect(x, 140, 106, 18, bg);
    textCenter(x, 146, 106, sorts[i], active ? C_GOLD : C_MUTED, bg, 1);
  }
  if (filter_ == Filter::Classic || filter_ == Filter::Cellular || filter_ == Filter::Mesh || filter_ == Filter::AdsB) {
    const char* need = filter_ == Filter::Classic ? "CLASSIC BLUETOOTH IS NOT SUPPORTED BY ESP32-C5" :
                       filter_ == Filter::Cellular ? "CELLULAR RECEIVER NOT CONNECTED" :
                       filter_ == Filter::Mesh ? "SUB-GHZ MESH RECEIVER NOT CONNECTED" :
                       "ADS-B RECEIVER NOT CONNECTED";
    text(25, 190, need, C_AMBER, C_PANEL, 1, 430);
    text(25, 213, "NO CONTACTS ARE GENERATED FOR THIS FILTER", C_MUTED, C_PANEL, 1, 430);
  } else {
    uint8_t indices[kMaxContacts];
    const uint8_t n = sortedContacts(state, indices);
    if (state.wifiScanFailed && filter_ != Filter::Ble) {
      text(25, 188, "PASSIVE SCAN FAILED", C_RED, C_PANEL, 2, 400);
    } else if (n == 0) {
      text(25, 188, state.wifiScanInProgress || state.bleScanInProgress ? "WAITING FOR LIVE OBSERVATIONS" : "NO LIVE CONTACTS", C_MUTED, C_PANEL, 1, 430);
    } else {
      const uint8_t visible = n > 3 ? 3 : n;
      for (uint8_t i = 0; i < visible; ++i) {
        const uint8_t index = indices[i];
        drawContactRow(state.contacts[index], 162 + i * 36, index == selectedContact_);
      }
      if (n > visible) {
        char more[32]; snprintf(more, sizeof(more), "+%u MORE", n - visible);
        text(380, 267, more, C_MUTED, C_PANEL, 1, 70);
      }
    }
  }
}

void WardriverUI::drawRadar(const AppState& state) {
  text(22, 93, "LIVE PROXIMITY / RSSI TIERS", C_GOLD, C_PANEL, 1, 430);
  uint8_t ids[kMaxContacts];
  const uint8_t n = sortedContacts(state, ids);
  if (n == 0) {
    text(25, 161, "RADAR IS EMPTY - NO LIVE CONTACTS", C_MUTED, C_PANEL, 1, 430);
    return;
  }
  const uint8_t visible = n > 4 ? 4 : n;
  for (uint8_t i = 0; i < visible; ++i) {
    const ContactObservation& c = state.contacts[ids[i]];
    const int16_t y = 119 + i * 36;
    const int16_t radius = c.rssiDbm > -50 ? 15 : (c.rssiDbm > -70 ? 10 : 6);
    drawCircle(48, y + 11, 15, C_LINE);
    fillCircle(48, y + 11, radius, c.rssiDbm > -55 ? C_GREEN : (c.rssiDbm > -75 ? C_GOLD : C_MUTED));
    text(78, y + 1, c.name, C_TEXT, C_PANEL, 1, 200);
    char line[42]; snprintf(line, sizeof(line), "%d DBM  |  %s", c.rssiDbm, c.protocol == ContactProtocol::Wifi ? "WIFI" : "BLE");
    text(78, y + 16, line, C_MUTED, C_PANEL, 1, 210);
  }
  text(325, 234, "RSSI ONLY", C_CYAN, C_PANEL, 1, 120);
  text(325, 251, "NO DISTANCE ESTIMATE", C_MUTED, C_PANEL, 1, 130);
}

void WardriverUI::drawFinds(const AppState& state) {
  text(22, 93, "GEO-TAGGED OBSERVATIONS", C_GOLD, C_PANEL, 1, 420);
  uint8_t count = 0;
  const uint8_t n = state.contactCount > kMaxContacts ? kMaxContacts : state.contactCount;
  for (uint8_t i = 0; i < n && count < 4; ++i) {
    const ContactObservation& c = state.contacts[i];
    if (!c.hasLocation) continue;
    const int16_t y = 118 + count * 35;
    text(24, y, c.name, C_TEXT, C_PANEL, 1, 180);
    char loc[60]; snprintf(loc, sizeof(loc), "%.5f, %.5f", c.latitude, c.longitude);
    text(220, y, loc, C_CYAN, C_PANEL, 1, 220);
    ++count;
  }
  if (count == 0) {
    text(24, 157, state.gps.hasFix ? "NO CONTACTS HAVE A STORED GPS POSITION" : "GPS FIX REQUIRED FOR GEO-TAGGED FINDS", C_MUTED, C_PANEL, 1, 430);
    text(24, 183, "THIS PAGE USES ONLY LOCATIONS IN THE CURRENT DATA", C_MUTED, C_PANEL, 1, 430);
  }
  text(24, 250, "HISTORICAL MAP REQUIRES SAVED LOG RECORDS", C_AMBER, C_PANEL, 1, 430);
}

void WardriverUI::drawSecurity(const AppState& state) {
  uint16_t counts[7] = {};
  const uint8_t n = state.contactCount > kMaxContacts ? kMaxContacts : state.contactCount;
  uint16_t wifiCount = 0;
  for (uint8_t i = 0; i < n; ++i) {
    if (state.contacts[i].protocol != ContactProtocol::Wifi) continue;
    ++wifiCount;
    const uint8_t security = authRank(state.contacts[i].security);
    if (security < 7) ++counts[security];
  }
  text(22, 93, "SECURITY CLASSIFICATION / PASSIVE OBSERVATIONS", C_GOLD, C_PANEL, 1, 435);
  const char* labels[] = {"UNKNOWN", "OPEN", "WEP", "WPA", "WPA2", "WPA3", "ENTERPRISE"};
  const uint16_t maxCount = *std::max_element(counts, counts + 7);
  for (uint8_t i = 0; i < 7; ++i) {
    const int16_t y = 116 + i * 20;
    text(24, y + 5, labels[i], C_TEXT, C_PANEL, 1, 110);
    fillRect(145, y + 5, 250, 7, C_PANEL_DARK);
    const int16_t w = maxCount == 0 ? 0 : static_cast<int16_t>(250U * counts[i] / maxCount);
    fillRect(145, y + 5, w, 7, i == 1 ? C_RED : C_CYAN);
    char value[10]; snprintf(value, sizeof(value), "%u", counts[i]);
    text(410, y + 5, value, C_GOLD, C_PANEL, 1, 30);
  }
  char total[48]; snprintf(total, sizeof(total), "WIFI CONTACTS OBSERVED: %u", wifiCount);
  text(24, 260, total, C_MUTED, C_PANEL, 1, 400);
}

void WardriverUI::drawChannels(const AppState& state, bool spectrum) {
  const int16_t top = 96;
  text(22, 93, spectrum ? "AP-COUNT CHANNEL VIEW" : "OBSERVED WIFI CHANNELS", C_GOLD, C_PANEL, 1, 430);
  uint8_t channels[40] = {};
  uint8_t counts[40] = {};
  uint8_t unique = 0;
  const uint8_t n = state.contactCount > kMaxContacts ? kMaxContacts : state.contactCount;
  for (uint8_t i = 0; i < n; ++i) {
    const ContactObservation& c = state.contacts[i];
    if (c.protocol != ContactProtocol::Wifi || c.channel == 0) continue;
    uint8_t j = 0;
    while (j < unique && channels[j] != c.channel) ++j;
    if (j == unique && unique < 40) { channels[unique] = c.channel; counts[unique] = 0; ++unique; }
    if (j < unique && counts[j] < 255) ++counts[j];
  }
  if (unique == 0) {
    text(24, 158, state.wifiScanInProgress ? "WAITING FOR PASSIVE WIFI RESULTS" : "NO WIFI CHANNEL DATA", C_MUTED, C_PANEL, 1, 430);
    return;
  }
  std::sort(channels, channels + unique);
  uint8_t max = 1;
  for (uint8_t i = 0; i < unique; ++i) {
    // Find each channel's count after sorting channel numbers.
    uint8_t count = 0;
    for (uint8_t j = 0; j < state.contactCount && j < kMaxContacts; ++j)
      if (state.contacts[j].protocol == ContactProtocol::Wifi && channels[i] == state.contacts[j].channel) ++count;
    if (count > max) max = count;
    counts[i] = count;
  }
  const uint8_t visible = unique > 6 ? 6 : unique;
  for (uint8_t i = 0; i < visible; ++i) {
    const int16_t y = top + 25 + i * 24;
    char label[12], number[12];
    snprintf(label, sizeof(label), "CH %u", channels[i]);
    snprintf(number, sizeof(number), "%u AP", counts[i]);
    text(24, y + 5, label, C_TEXT, C_PANEL, 1, 50);
    fillRect(86, y + 5, 270, 8, C_PANEL_DARK);
    fillRect(86, y + 5, max ? (270 * counts[i] / max) : 0, 8, C_CYAN);
    text(370, y + 5, number, C_MUTED, C_PANEL, 1, 85);
  }
  if (spectrum) text(24, 260, "AP COUNTS ONLY - NO RF POWER MEASUREMENT", C_AMBER, C_PANEL, 1, 430);
  else text(24, 260, "COUNTS ARE FROM CURRENT WIFI OBSERVATIONS", C_MUTED, C_PANEL, 1, 430);
}

void WardriverUI::drawField(const AppState& state) {
  if (selectedContact_ < 0 || selectedContact_ >= state.contactCount) {
    text(22, 101, "FIELD DETAIL", C_GOLD, C_PANEL, 1, 200);
    text(22, 155, "TAP A LIVE CONTACT TO OPEN ITS DETAIL", C_MUTED, C_PANEL, 1, 430);
    return;
  }
  const ContactObservation& c = state.contacts[selectedContact_];
  text(22, 93, "OBSERVED DEVICE / NO VENDOR GUESS", C_GOLD, C_PANEL, 1, 430);
  text(24, 121, "NAME", C_MUTED, C_PANEL, 1, 75);
  text(130, 121, c.name[0] ? c.name : "UNNAMED", C_TEXT, C_PANEL, 1, 320);
  text(24, 149, c.protocol == ContactProtocol::Wifi ? "BSSID" : "ADDRESS", C_MUTED, C_PANEL, 1, 75);
  text(130, 149, c.address, C_TEXT, C_PANEL, 1, 320);
  char line[64];
  snprintf(line, sizeof(line), "RSSI %d DBM  |  SEEN %u TIMES", c.rssiDbm, c.sightings);
  text(24, 177, line, C_CYAN, C_PANEL, 1, 430);
  if (c.protocol == ContactProtocol::Wifi) {
    snprintf(line, sizeof(line), "CHANNEL %u  |  %s", c.channel, securityName(c.security));
    text(24, 201, line, C_TEXT, C_PANEL, 1, 430);
  } else text(24, 201, "BLE ADVERTISEMENT", C_TEXT, C_PANEL, 1, 430);
  if (c.hasLocation) {
    snprintf(line, sizeof(line), "%.5f, %.5f", c.latitude, c.longitude);
    text(24, 225, line, C_CYAN, C_PANEL, 1, 430);
  } else text(24, 225, "NO GPS TAG ON THIS OBSERVATION", C_MUTED, C_PANEL, 1, 430);
  text(24, 251, "LONG-PRESS CONTACT IN HUNT FOR FOXHUNT", C_AMBER, C_PANEL, 1, 430);
}

void WardriverUI::drawTools(const AppState& state) {
  text(22, 93, "RECEIVER / DEVICE STATUS", C_GOLD, C_PANEL, 1, 430);
  const char* names[] = {"WIFI 2.4 / 5 GHZ", "BLUETOOTH LE", "CLASSIC BLUETOOTH", "CELLULAR", "SUB-GHZ MESH", "ADS-B"};
  const bool ready[] = {state.wifiReady, state.bleAvailable, false, false, false, false};
  for (uint8_t i = 0; i < 6; ++i) {
    const int16_t y = 118 + i * 23;
    fillRect(22, y, 434, 20, C_PANEL_DARK);
    text(30, y + 6, names[i], C_TEXT, C_PANEL_DARK, 1, 220);
    const char* status = ready[i] ? "READY" :
        (i == 2 ? "NOT ON ESP32-C5" : (i == 0 || i == 1 ? "UNAVAILABLE" : "RECEIVER NEEDED"));
    text(310, y + 6, status, ready[i] ? C_GREEN : C_AMBER, C_PANEL_DARK, 1, 140);
  }
}

void WardriverUI::drawAlerts(const AppState&) {
  text(22, 93, "ALERTS", C_GOLD, C_PANEL, 1, 430);
  text(24, 145, "NO DEVICE SIGNATURE DATABASE LOADED", C_MUTED, C_PANEL, 1, 430);
  text(24, 170, "CLASSIFICATION ALERTS ARE DISABLED", C_AMBER, C_PANEL, 1, 430);
  text(24, 208, "OBSERVATIONS REMAIN AVAILABLE IN HUNT", C_CYAN, C_PANEL, 1, 430);
}

void WardriverUI::drawPrivacy() {
  text(22, 93, "LOCAL PRIVACY CONTROLS", C_GOLD, C_PANEL, 1, 430);
  fillRect(24, 128, 430, 42, C_PANEL_DARK);
  drawRect(24, 128, 430, 42, honorNomap_ ? C_GREEN : C_LINE);
  text(38, 145, "HONOR _NOMAP / _OPTOUT", C_TEXT, C_PANEL_DARK, 1, 290);
  fillRect(390, 138, 48, 22, honorNomap_ ? 0x124C : 0x3126);
  textCenter(391, 145, 46, honorNomap_ ? "ON" : "OFF", honorNomap_ ? C_GREEN : C_AMBER,
             honorNomap_ ? 0x124C : 0x3126, 1);
  text(24, 190, "LOCAL BLACKLIST: NO ENTRIES LOADED", C_MUTED, C_PANEL, 1, 430);
  text(24, 218, "EXCLUSION ZONES: NO ZONES LOADED", C_MUTED, C_PANEL, 1, 430);
  text(24, 251, "SETTING IS LOCAL; NO UPLOAD OCCURS HERE", C_CYAN, C_PANEL, 1, 430);
}

void WardriverUI::drawTransfer(const AppState& state) {
  text(22, 93, "LOCAL EXPORT", C_GOLD, C_PANEL, 1, 430);
  fillRect(24, 119, 430, 54, C_PANEL_DARK);
  drawRect(24, 119, 430, 54, state.sdAvailable ? C_CYAN : C_RED);
  text(40, 132, state.sdAvailable ? "EXPORT OBSERVATIONS TO SD" : "SD CARD NOT AVAILABLE", C_TEXT, C_PANEL_DARK, 1, 380);
  text(40, 151, state.sdAvailable ? "CSV EXPORT IS LOCAL" : "INSERT / MOUNT A CARD FIRST", C_MUTED, C_PANEL_DARK, 1, 380);
  fillRect(24, 187, 180, 35, state.sdAvailable ? 0x10C6 : C_PANEL_DARK);
  drawRect(24, 187, 180, 35, state.sdAvailable ? C_CYAN : C_LINE);
  textCenter(25, 201, 178, "EXPORT CSV", state.sdAvailable ? C_TEXT : C_MUTED, state.sdAvailable ? 0x10C6 : C_PANEL_DARK, 1);
  text(24, 245, "UPLOADS DISABLED - NO NETWORK KEYS OR ENDPOINTS", C_MUTED, C_PANEL, 1, 430);
}

void WardriverUI::drawFoxhunt(const AppState& state) {
  if (selectedContact_ < 0 || selectedContact_ >= state.contactCount) {
    text(24, 141, "NO LIVE CONTACT SELECTED", C_AMBER, C_PANEL, 1, 420);
    text(24, 170, "RETURN TO HUNT AND LONG-PRESS A CONTACT", C_MUTED, C_PANEL, 1, 420);
    return;
  }
  const ContactObservation& c = state.contacts[selectedContact_];
  text(22, 94, "FOXHUNT / SIGNAL ONLY", C_PINK, C_PANEL, 1, 430);
  text(24, 119, c.name[0] ? c.name : "UNNAMED", C_TEXT, C_PANEL, 2, 400);
  char line[56];
  snprintf(line, sizeof(line), "%d DBM   %s", c.rssiDbm, c.protocol == ContactProtocol::Wifi ? "WIFI" : "BLE");
  text(24, 151, line, C_CYAN, C_PANEL, 1, 420);
  const int16_t strength = c.rssiDbm <= -100 ? 0 : c.rssiDbm >= -30 ? 100 : (c.rssiDbm + 100) * 100 / 70;
  fillRect(24, 179, 420, 14, C_PANEL_DARK);
  fillRect(24, 179, 420 * strength / 100, 14, c.rssiDbm > -55 ? C_GREEN : (c.rssiDbm > -75 ? C_GOLD : C_MUTED));
  text(24, 202, "CUE MODE", C_MUTED, C_PANEL, 1, 100);
  const char* cues[] = {"SILENT", "BEEP", "VIBE", "BOTH"};
  for (uint8_t i = 0; i < 4; ++i) {
    const int16_t x = 24 + i * 108;
    const uint16_t bg = static_cast<uint8_t>(cueMode_) == i ? 0x3126 : C_PANEL_DARK;
    fillRect(x, 221, 100, 28, bg);
    drawRect(x, 221, 100, 28, static_cast<uint8_t>(cueMode_) == i ? C_PINK : C_LINE);
    textCenter(x, 232, 100, cues[i], C_TEXT, bg, 1);
  }
  char status[64];
  snprintf(status, sizeof(status), "BEEP %s  |  VIBRATION %s",
           state.beepAvailable ? "READY" : "NOT CONFIGURED",
           state.vibrationAvailable ? "READY" : "NOT CONFIGURED");
  text(24, 258, status, C_MUTED, C_PANEL, 1, 430);
}

bool WardriverUI::draw(const AppState& state, bool force) {
  if (!spi_) return false;
  const uint32_t now = millis();
  if (!force && !dirty_ && now - lastDrawMs_ < 700) return false;
  fillScreen(C_BG);
  if (!state.labAcknowledged) {
    drawLabGate(state);
  } else switch (page_) {
    case Page::Home: drawHome(state); break;
    case Page::SdBrowser: drawSdBrowser(state); break;
    case Page::GpsDiagnostics:
      drawChrome(state, "MISS DIRECTION / GPS");
      fillRect(12, 52, 456, 221, C_PANEL);
      drawRect(12, 52, 456, 221, C_LINE);
      text(24, 62, "GNSS DIAGNOSTICS", C_GOLD, C_PANEL, 1, 420);
      drawGps(state, false);
      drawDock();
      break;
    case Page::Wardrive: drawWardrive(state); break;
  }
  dirty_ = false;
  lastDrawMs_ = now;
  return true;
}

UiAction WardriverUI::handleTouch(uint16_t x, uint16_t y, uint32_t heldMs,
                                  const AppState& state) {
  if (!state.labAcknowledged) {
    if (y >= 160 && y < 210) {
      dirty_ = true;
      return UiAction::ToggleLabCheck;
    }
    if (y >= 245 && y < 288 && state.labCheckboxChecked) {
      dirty_ = true;
      return UiAction::ConfirmLabUse;
    }
    return UiAction::None;
  }
  if (y >= kDockY) {
    foxhunting_ = false;
    if (x < 160) { page_ = Page::Home; dirty_ = true; return UiAction::OpenHome; }
    if (x < 320) { page_ = Page::Wardrive; tab_ = Tab::Hunt; dirty_ = true; return UiAction::OpenWardrive; }
    page_ = Page::SdBrowser; dirty_ = true; return UiAction::OpenSdBrowser;
  }
  if (page_ == Page::Home) {
    if (y >= 52 && y < 178) {
      page_ = Page::GpsDiagnostics; dirty_ = true; return UiAction::OpenGpsDiagnostics;
    }
    if (y >= 190 && y < 274) {
      if (x < 240) { page_ = Page::Wardrive; tab_ = Tab::Hunt; dirty_ = true; return UiAction::OpenWardrive; }
      page_ = Page::SdBrowser; dirty_ = true; return UiAction::OpenSdBrowser;
    }
    return UiAction::None;
  }
  if (page_ == Page::SdBrowser) {
    const uint8_t count = state.sdEntryCount > kMaxSdEntries ? kMaxSdEntries : state.sdEntryCount;
    if (state.sdAvailable && y >= 133 && y < 265) {
      const uint8_t index = static_cast<uint8_t>((y - 133) / 22);
      if (index < count) { selectedSdEntry_ = index; dirty_ = true; return UiAction::OpenSdFile; }
    }
    if (y < 82) { page_ = Page::Home; dirty_ = true; return UiAction::OpenHome; }
    return UiAction::None;
  }
  if (page_ == Page::GpsDiagnostics) {
    if (y < 82) { page_ = Page::Wardrive; tab_ = Tab::Gps; dirty_ = true; return UiAction::None; }
    return UiAction::None;
  }
  if (page_ != Page::Wardrive) return UiAction::None;
  if (foxhunting_) {
    if (y < 82) { foxhunting_ = false; tab_ = Tab::Hunt; dirty_ = true; return UiAction::None; }
    if (y >= 221 && y < 253) {
      const uint8_t cue = x < 132 ? 0 : x < 240 ? 1 : x < 348 ? 2 : 3;
      cueMode_ = static_cast<CueMode>(cue);
      dirty_ = true;
      switch (cueMode_) {
        case CueMode::Silent: return UiAction::SetCueSilent;
        case CueMode::Beep: return UiAction::SetCueBeep;
        case CueMode::Vibration: return UiAction::SetCueVibration;
        case CueMode::Both: return UiAction::SetCueBoth;
      }
    }
    return UiAction::None;
  }
  if (y >= kTabY && y < kTabY + kTabH) {
    if (x < 31) { tabStart_ = tabStart_ > 4 ? tabStart_ - 4 : 0; dirty_ = true; return UiAction::None; }
    if (x > 450) { tabStart_ = tabStart_ + 4 > kTabCount - kVisibleTabs ? kTabCount - kVisibleTabs : tabStart_ + 4; dirty_ = true; return UiAction::None; }
    const uint8_t slot = static_cast<uint8_t>((x - kTabX) / kTabWidth);
    if (slot < kVisibleTabs && tabStart_ + slot < kTabCount) {
      tab_ = static_cast<Tab>(tabStart_ + slot); dirty_ = true; return UiAction::None;
    }
  }
  if (tab_ == Tab::Hunt) {
    if (y >= 88 && y < 113 && x >= 365) return state.wifiScanInProgress ? UiAction::None : UiAction::StartPassiveScan;
    if (y >= 116 && y < 138) {
      const uint8_t idx = (x - 17) / 63;
      if (idx < 7) { filter_ = static_cast<Filter>(idx); dirty_ = true; }
      return UiAction::None;
    }
    if (y >= 140 && y < 158) {
      const uint8_t idx = (x - 18) / 111;
      if (idx < 4) { sort_ = static_cast<Sort>(idx); dirty_ = true; }
      return UiAction::None;
    }
    if (y >= 162 && y < 270) {
      uint8_t indices[kMaxContacts];
      const uint8_t n = sortedContacts(state, indices);
      const uint8_t row = static_cast<uint8_t>((y - 162) / 36);
      if (row < n && row < 3) {
        selectedContact_ = indices[row];
        if (heldMs >= 600) foxhunting_ = true;
        else tab_ = Tab::Field;
        dirty_ = true;
      }
      return UiAction::None;
    }
  } else if (tab_ == Tab::Privacy && y >= 128 && y < 170) {
    honorNomap_ = !honorNomap_;
    dirty_ = true;
    return UiAction::TogglePrivacyNomap;
  } else if (tab_ == Tab::Transfer && y >= 187 && y < 223) {
    return state.sdAvailable ? UiAction::ExportCsv : UiAction::OpenSdBrowser;
  } else if (tab_ == Tab::Gps && y >= 100) {
    page_ = Page::GpsDiagnostics;
    dirty_ = true;
    return UiAction::OpenGpsDiagnostics;
  }
  return UiAction::None;
}

void WardriverUI::drawCalibrationTarget(uint8_t index, uint8_t total,
                                        uint16_t targetX, uint16_t targetY) {
  if (!spi_) return;
  fillScreen(C_BG);
  fillRect(0, 0, kWidth, 43, C_PANEL_DARK);
  text(18, 9, "TOUCH CALIBRATION", C_GOLD, C_PANEL_DARK, 2, 440);
  char line[40]; snprintf(line, sizeof(line), "POINT %u OF %u", index + 1, total);
  textCenter(16, 70, 448, line, C_TEXT, C_BG, 2);
  const int16_t x = targetX, y = targetY;
  drawCircle(x, y, 15, C_GOLD);
  drawCircle(x, y, 7, C_CYAN);
  drawLine(x - 22, y, x + 22, y, C_TEXT);
  drawLine(x, y - 22, x, y + 22, C_TEXT);
  textCenter(18, 208, 444, "TAP THE CENTER OF THE CROSSHAIR", C_MUTED, C_BG, 1);
  dirty_ = true;
}

void WardriverUI::drawCalibrationError() {
  if (!spi_) return;
  fillScreen(C_BG);
  fillRect(0, 0, kWidth, 45, C_PANEL_DARK);
  text(18, 12, "TOUCH CALIBRATION FAILED", C_RED, C_PANEL_DARK, 2, 440);
  textCenter(20, 115, 440, "NO CALIBRATION WAS SAVED", C_TEXT, C_BG, 1);
  textCenter(20, 145, 440, "CHECK TOUCH CS / SPI CONNECTIONS", C_AMBER, C_BG, 1);
  textCenter(20, 174, 440, "RESTART TO TRY THE FOUR POINTS AGAIN", C_MUTED, C_BG, 1);
  dirty_ = true;
}

