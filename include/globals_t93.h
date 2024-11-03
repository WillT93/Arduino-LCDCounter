#ifndef _T93_LCD_COUNTER_GLOBALS_h
#define _T93_LCD_COUNTER_GLOBALS_h

#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

#include "enums_t93.h"

// Debugging configuration
#define DEBUG                 true      // Optional printing of debug messages to serial.
#define EEPROM_INIT           false     // When set to true, EEPROM is written over with 0's. Perform once per ESP32 unit.

// Helper methods
#define LEN(arr)              ((int) (sizeof (arr) / sizeof (arr)[0]))
#define DEBUG_SERIAL          if (DEBUG) Serial

// LCD
#define ANIM_FRAME_COUNT      8     // The number of frames in the LCD animation sequence.
#define LCD_COLUMNS           16    // Number of columns in the LCD.
#define LCD_ROWS              2     // Number of rows in the LCD.
#define LCD_ADDRESS           0x27  // The I2C address the LCD lives at. Can be found using an I2C scanning sketch.

// Buttons
#define BTN_1_PIN             34    // The input pin the first button is connected to.
#define BTN_2_PIN             35    // The input pin the second button is connected to.

// LDR
#define LDR_PIN               32    // The input pin the LDR is connected to, must be capable of analog input reading.
#define LDR_DARK_ROOM_THRESH  200   // The value the LDR must be 50 below to turn the backlight off and 50 above to turn the backlight on.

// WiFi / API
#define WIFI_RECONN_TIMEOUT   10    // How long to attempt WiFi connection with saved credentials before invoking portal. Also how often it will wait between re-attempts when portal is running.
#define POLL_INTERVAL_SECONDS 30    // How often to poll the endpoint.
#define API_VALUE_COUNT       3     // The number of values this version of code expects from the API, values in excess will be discarded. There should be a _valueLabel entry for each of these in secrets file.
#define RESPONSE_BUFFFER_SIZE 512   // The size of the buffer to read the API response string into. Must be at least as large as the length of the returned payload.
#define MAX_VALUE_LENGTH      16    // The maximum length of each return value including termination character. Note only allowing up to 15 chars (plus termination) because the 16th column is used for the folling indicator.

#define RESTART_INTERVAL      86,400,000  // The number of milliseconds in 24 hours, used for periodic reboots to keep memory fresh.

extern hd44780_I2Cexp _lcd;
extern char _currentValue[API_VALUE_COUNT][MAX_VALUE_LENGTH];       // The current values available to be rendered on the display. One for each API value. Length (incl termination char) is up to the number of columns on the LCD as the last column is reserved for API polling indicator.
extern bool _currentValueUpdated[API_VALUE_COUNT];                  // Whether the latest value received from the API differs from what is currently being rendered. One for each API value.
extern int _selectedValueIndex;                                     // The statistic chosen to be displayed. API returns multiple, pipe delimited ints. The one selected here is what is rendered on the display.
extern DisplayDimmingMode _selectedDisplayMode;                     // How the display backlight should behave when the device is in a dark room.
extern bool _lcdBacklightOn;                                        // Whether the LCD backlight is on.

#endif