#include <EEPROM.h>

#include "globals_t93.h"
#include "enums_t93.h"
#include "lcd_t93.h"
#include "eeprom_t93.h"

void InitializeEEPROM() {
  DEBUG_SERIAL.print("Initializing EEPROM with a size of ");
  DEBUG_SERIAL.println(EEPROM_SIZE);
  EEPROM.begin(EEPROM_SIZE);

  if (EEPROM_INIT) {
    ClearEEPROM();
  }

  LoadConfigFromEEPROM();
}

/*
* Pulls configuration from EEPROM on ESP startup and reads them into the global variables.
*/
void LoadConfigFromEEPROM() {
  DEBUG_SERIAL.println("Loading config values from EEPROM");
  _selectedValueIndex = EEPROM.readInt(SV_INDEX);
  _selectedDisplayMode = static_cast<DisplayDimmingMode>(EEPROM.readInt(DM_INDEX));
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

  EEPROM.writeInt(SV_INDEX, _selectedValueIndex);
  EEPROM.writeInt(DM_INDEX, _selectedDisplayMode);
  EEPROM.commit();

  DEBUG_SERIAL.println("Saved config values to EEPROM");
  DEBUG_SERIAL.print("_selectedValueIndex: ");
  DEBUG_SERIAL.println(EEPROM.readInt(SV_INDEX));
  DEBUG_SERIAL.print("_selectedDisplayMode: ");
  DEBUG_SERIAL.println(EEPROM.readInt(DM_INDEX));
}

/*
* Clears the EEPROM, sets all addresses to 0 and then loads in default config.
*/
void ClearEEPROM() {
  DEBUG_SERIAL.println("Clearing EEPROM and writing with 0 values");
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  };
  EEPROM.commit();
}