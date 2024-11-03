#include <EEPROM.h>

#include "globals_t93.h"
#include "enums_t93.h"
#include "lcd_t93.h"

void InitializeEEPROM() {
  DEBUG_SERIAL.print("Initializing EEPROM with a size of ");
  DEBUG_SERIAL.println(EEPROM_SIZE);
  EEPROM.begin(EEPROM_SIZE);
}

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
  EEPROM.commit();

  DEBUG_SERIAL.println("Saved config values to EEPROM");
  DEBUG_SERIAL.print("_selectedValueIndex: ");
  DEBUG_SERIAL.println(EEPROM.readInt(0));
  DEBUG_SERIAL.print("_selectedDisplayMode: ");
  DEBUG_SERIAL.println(EEPROM.readInt(1));
}

/*
* Clears the EEPROM, sets all addresses to 0 and then loads in default config.
*/
void ClearEEPROM() {
  DEBUG_SERIAL.println("Clearing EEPROM and writing with 0 values");
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  };
  SaveConfigToEEPROM();
}