#pragma once

#include <Arduino.h>

struct BoardPins {
  int8_t spiSck = D8;       // GPIO8
  int8_t spiMiso = D9;      // GPIO9
  int8_t spiMosi = D10;     // GPIO10
  int8_t tftCs = D2;        // GPIO25 (strap input at reset; TFT CS is high impedance)
  int8_t tftDc = D3;        // GPIO7 (strap input at reset; TFT DC is high impedance)
  int8_t tftReset = -1;     // display RESET is tied high to 3V3
  int8_t touchCs = D6;      // GPIO11
  int8_t touchIrq = -1;     // touch is polled; no XIAO pin allocated
  int8_t sdCs = D4;         // GPIO23
  int8_t gpsRx = D7;        // GPIO12; GPS TX -> XIAO RX only
  int8_t gpsTx = -1;        // GPS RX is not connected
  int8_t nrfCe = D0;        // GPIO1, preserved from the wiring diagram
  int8_t nrfCsn = D1;       // GPIO0, preserved from the wiring diagram
  int8_t ccCs = D5;         // GPIO24, preserved from the wiring diagram
};

