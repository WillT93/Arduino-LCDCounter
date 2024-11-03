#include <Arduino.h>
#include <elapsedMillis.h>

#include "globals_t93.h"
#include "secrets_t93.h"
#include "lcd_t93.h"
#include "eeprom_t93.h"
#include "enums_t93.h"
#include "ldr_t93.h"

/*
* Configures pinMode for the LDR pin and initial calibration.
*/
void InitializeLDR() {
  DEBUG_SERIAL.println("Initializing LDR");
  pinMode(LDR_PIN, INPUT);
  DEBUG_SERIAL.println("LDR Initialized");
}

/*
* Non-blocking check on the status of the LDR.
* If Auto backlight configured, turns LCD backlight off in a dark toom.
*/
void ProcessLDR() {
  if (_selectedDisplayMode == Auto) {
    UpdateBacklightPerLightLevel();
  }
}

/*
* Non-blocking check on the brightness level of the room.
* If the light level in the room is below a certain threshold, the backlight will be turned off.
* Likewise if the light level is above a certain threshold, the backlight will be turned on.
* A delta exists between these thresholds to prevent flipping back and fourth over the line between light and dark.
* Debouncing is also used to prevent flicking back and fourth over each threshold.
*/
void UpdateBacklightPerLightLevel() {
  static const int darknessTimeThreshold = 2500;                            // After this time (ms) it is unlikely to be a thumb swipe and more likely to be the lights turning off.
  static const int debounceDelay = 50;                                      // How long the LDR must read a consistant high or low value to be considered stable input.

  static elapsedMillis debounceTimer = 0;                                   // Timer for debouncing LDR readings.
  static bool previouslyReadingDarkness;                                    // LDR reading from the previous invocation. Used for debouncing, not for tracking complete state changes.
  static bool previouslyReadingLightness;                                   // LDR reading from the previous invocation. Used for debouncing, not for tracking complete state changes.

  static elapsedMillis darkTimer = 0;                                       // The time since the LDR read below the dark room threshold.
  static bool previouslyInDarkRoom = false;                                 // Tracks if LDR has been in stable darkness. Not to be confused with previouslyReadingDarkness.
  
  bool readingBelowDarkRoomThreshold = LDRBelowDarkRoomThreshold();
  bool readingAboveLightRoomThreshold = LDRAboveLightRoomThreshold();

  if (readingBelowDarkRoomThreshold != previouslyReadingDarkness) {         // Debounces moving above and below the lower threshold.
    DEBUG_SERIAL.print("LDR dark room state changed. Now with value of: ");
    DEBUG_SERIAL.println(analogRead(LDR_PIN));
    debounceTimer = 0;
  }

  if (readingAboveLightRoomThreshold != previouslyReadingLightness) {       // Debounces moving above and below the upper threshold.
    DEBUG_SERIAL.print("LDR light room state changed. Now with value of: ");
    DEBUG_SERIAL.println(analogRead(LDR_PIN));
    debounceTimer = 0;
  }

  if (debounceTimer > debounceDelay) {                                      // Readings have stabilized.
    if (readingBelowDarkRoomThreshold && !previouslyInDarkRoom) {           // If moving from above the lower threshold to below it...
      DEBUG_SERIAL.println("Moving from light state to dark state");
      previouslyInDarkRoom = true;                                          // Have moved into a dark state.
      darkTimer = 0;                                                        // Begin tracking how long the unit has been in darkness.
    }

    if (readingBelowDarkRoomThreshold && previouslyInDarkRoom) {            // Currently in darkness, and have been for a while.
      if (darkTimer > darknessTimeThreshold && _lcdBacklightOn) {           // Have been in darkness long enough to now turn off backlight.
        DEBUG_SERIAL.println("Was in dark state long enough to consider the room moving into dark state");
        DEBUG_SERIAL.println("LCD in auto mode, turning off backlight");
        _lcd.noBacklight();
        _lcdBacklightOn = false;
      }
    }

    if (readingAboveLightRoomThreshold && previouslyInDarkRoom) {           // If moving from below the upper threshold to above it...
      DEBUG_SERIAL.println("Moving from dark state to light state");
      previouslyInDarkRoom = false;                                         // Reset for next loop.
      if (darkTimer > darknessTimeThreshold && !_lcdBacklightOn) {          // Was in darkness for more than 3000ms, likely lights were off and now are back on.
        DEBUG_SERIAL.println("Was in dark state long enough to consider the room previously being dark");
        DEBUG_SERIAL.println("LCD in auto mode, turning on backlight");
        _lcd.backlight();
        _lcdBacklightOn = true;
      }
    }
  }

  previouslyReadingDarkness = readingBelowDarkRoomThreshold;                // Update values for next loop debouncing.
  previouslyReadingLightness = readingAboveLightRoomThreshold;
}

/*
* True if the LDR is reading a value below the dark room threshold (with a margin).
* This indicates the room is dark enough to turn off the backlight when the LCD is in Auto mode.
*/
bool LDRBelowDarkRoomThreshold() {
  return analogRead(LDR_PIN) <= max(LDR_DARK_ROOM_THRESH - 50, 0);
}

/*
* True if the LDR is reading a value above the light room threshold (with a margin).
* This indicates the room is light enough to turn on the backlight when the LCD is in Auto mode.
*/
bool LDRAboveLightRoomThreshold() {
  return analogRead(LDR_PIN) >= min(LDR_DARK_ROOM_THRESH + 50, 4095);
}