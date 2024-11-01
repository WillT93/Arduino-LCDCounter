#include <EEPROM.h>

#include "globals_t93.h"
#include "enums_t93.h"
#include "lcd_t93.h"

/*
* Pulls configuration from EEPROM on ESP startup and reads them into the global variables.
*/
void LoadConfigFromEEPROM() {
  _selectedValueIndex = EEPROM.readInt(0);
  _selectedDisplayMode = static_cast<DisplayDimmingMode>(EEPROM.readInt(1));
}

/*
* Saves configuration to EEPROM when config edited via bluetooth command.
*/
void SaveConfigToEEPROM() {
  EEPROM.writeInt(0, _selectedValueIndex);
  EEPROM.writeInt(1, _selectedDisplayMode);
}

/*
* Initializes the EEPROM, sets all addresses to 0 and then loads in default config.
*/
void InitializeEEPROM() {
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  };
  SaveConfigToEEPROM();
}