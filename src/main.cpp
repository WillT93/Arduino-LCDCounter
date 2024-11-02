// TODO: LDR display blanking. (needs testing)
// TODO: LDR value selection.  (needs testing)
// TODO: Run packet analysis.

#include <Arduino.h>

#include "api_t93.h"
#include "controls_t93.h"
#include "eeprom_t93.h"
#include "enums_t93.h"
#include "globals_t93.h"
#include "lcd_t93.h"
#include "secrets_t93.h"
#include "wifi_t93.h"

void setup() {
  if (EEPROM_INIT) {
    InitializeEEPROM();
    return;
  }

  DEBUG_SERIAL.begin(9600);

  LoadConfigFromEEPROM();
  InitializeInputDevices();
  InitializeLCD();
  InitializeWiFi();
}

void loop() {
  ProcessAPIPolling();
  ProcessDisplayValueUpdate();
  ProcessButtons();
  ProcessLDR();
}