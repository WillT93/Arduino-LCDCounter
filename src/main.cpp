// TODO: LDR display blanking. (needs testing)
// TODO: LDR value selection.  (needs testing)
// TODO: Configuration cycling for different LDR modes. (Needs testing)
// TODO: Run packet analysis.
// TODO: More serial debug logs.
// TODO: More inline comments.
// TODO: break into smaller .cpp and .h.
// TODO: look into warnings about array bounds

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
  ProcessButtons();
  ProcessLDR();
}