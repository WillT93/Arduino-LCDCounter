#ifndef _T93_LCD_COUNTER_ENUMS_h
#define _T93_LCD_COUNTER_ENUMS_h

enum DisplayDimmingMode {
  Auto = 0,   // Display backlight will turn itself off when in a dark room.
  On = 1,     // Display backlight is always on.
  Off = 2     // Display backlight is always off.
};

enum ButtonActionResult {
  None = 0,           // No button action took place
  Push = 1,           // The button was pushed in
  QuickPress = 2,   // The button was released after being pressed briefly
  HoldPress = 3     // The button was released after being held
};

#endif