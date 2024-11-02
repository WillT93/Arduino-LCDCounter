#include <EEPROM.h>

#include "globals_t93.h"
#include "enums_t93.h"
#include "lcd_t93.h"

/*
* Pulls configuration from EEPROM on ESP startup and reads them into the global variables.
*/
void LoadConfigFromEEPROM() {
  DEBUG_SERIAL.println("Loading config values from EEPROM");
  _selectedValueIndex = EEPROM.readInt(0);
  _selectedDisplayMode = static_cast<DisplayDimmingMode>(EEPROM.readInt(1));
  DEBUG_SERIAL.print("_selectedValueIndex: ");
  DEBUG_SERIAL.println(_selectedValueIndex);
  DEBUG_SERIAL.print("_selectedDisplayMode: ");
  DEBUG_SERIAL.println(_selectedDisplayMode);
}

/*
* Saves configuration to EEPROM when config edited via bluetooth command.
*/
void SaveConfigToEEPROM() {
  DEBUG_SERIAL.println("Saving config values to EEPROM");
  DEBUG_SERIAL.print("_selectedValueIndex: ");
  DEBUG_SERIAL.println(_selectedValueIndex);
  DEBUG_SERIAL.print("_selectedDisplayMode: ");
  DEBUG_SERIAL.println(_selectedDisplayMode);
  EEPROM.writeInt(0, _selectedValueIndex);
  EEPROM.writeInt(1, _selectedDisplayMode);
}

/*
* Initializes the EEPROM, sets all addresses to 0 and then loads in default config.
*/
void InitializeEEPROM() {
  DEBUG_SERIAL.println("Initializing EEPROM with 0 values");
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  };
  SaveConfigToEEPROM();
}