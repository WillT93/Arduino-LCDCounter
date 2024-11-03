#include <Arduino.h>
#include <elapsedMillis.h>

#include "api_t93.h"
#include "buttons_t93.h"
#include "eeprom_t93.h"
#include "enums_t93.h"
#include "globals_t93.h"
#include "lcd_t93.h"
#include "ldr_t93.h"
#include "secrets_t93.h"
#include "wifi_t93.h"

elapsedMillis memoryLoggingTimer;
elapsedMillis restartTimer;

void LogMemoryUsage();
void ProcessRestart();

void setup() {
  if (EEPROM_INIT) {
    ClearEEPROM();
    return;
  }

  DEBUG_SERIAL.begin(9600);

  InitializeEEPROM();
  LoadConfigFromEEPROM();
  InitializeButtons();
  InitializeLDR();
  InitializeLCD();
  InitializeWiFi();

  if (DEBUG) {
    memoryLoggingTimer = 0;
  }

  restartTimer = 0;
}

void loop() {
  ProcessAPIPolling();
  ProcessDisplayValueUpdate();
  ProcessButtons();
  ProcessLDR();
  if (DEBUG && memoryLoggingTimer > 10000) {
    LogMemoryUsage();
    memoryLoggingTimer = 0;
  }
  ProcessRestart();
}

void LogMemoryUsage() {
  size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
  size_t largestFreeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
  DEBUG_SERIAL.printf("Free Heap: %zu, Largest Free Block: %zu\n", freeHeap, largestFreeBlock);
}

void ProcessRestart() {
  if (restartTimer > restartTimer) {
    DEBUG_SERIAL.println("Periodic reboot of ESP32 to keep memory fresh.");
    WriteToLCD("Periodic reboot", "cycle commencing");
    delay(1000);
    ESP.restart();
  }
}