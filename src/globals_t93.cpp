#include "globals_t93.h"

hd44780_I2Cexp _lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
char _currentValue[API_VALUE_COUNT][MAX_VALUE_LENGTH];
bool _currentValueUpdated[API_VALUE_COUNT];
int _selectedValueIndex;
DisplayDimmingMode _selectedDisplayMode;
bool _lcdBacklightOn;