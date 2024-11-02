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
  CalibrateLDRSwipeThreshold();

  DEBUG_SERIAL.println("LDR Initialized");
}

/*
* Non-blocking check on the status of the LDR.
* Used to check whether the user is swiping or has swiped across the sensor, as well as the current level of room brightness.
* If user swipe is detected, LDRSwiped() is called. Otherwise if the room is in a dark state, the backlight will be turned off if configured to do so.
*/
void ProcessLDR() {
  static const int recalibrationInterval = 60000;                   // The interval with which the LDR will recalibrate it's value. This will also happen when it detects the lights turning on and off.

  static elapsedMillis recalibrationTimer = 0;                      // Timer to track recalibration runs.

  if (recalibrationTimer > recalibrationInterval) {                 // Periodically update the swipe threshold to account for gradual changes in room brightness throughout the day.
    CalibrateLDRSwipeThreshold();
  }

  if (_selectedDisplayMode == Auto) {
    UpdateBacklightPerLightLevel();
  }

  ProcessSwiping();
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
* Updates the swipe threshold the LDR must be reading below to consider a thumb swipe to be taking place.
* Percentage decrease must be at least 100pts, otherwise 100pts is used as threshold delta.
* Cannot be lower than 0.
*/
void CalibrateLDRSwipeThreshold() {
  DEBUG_SERIAL.println("Calibrating LDR");
  int currentValue = analogRead(LDR_PIN);
  _ldrSwipeThreshold = max(0, min((int)(currentValue * 0.75), currentValue - 100));     // Set to the lesser of 15% below it's current value or 100pts below, not going any lower than 0.
  DEBUG_SERIAL.print("LDR calibrated with swipe threshold of: ");
  DEBUG_SERIAL.println(_ldrSwipeThreshold);
}

/*
* True if the LDR is reading a value below the dark room threshold (with a margin).
* This indicates the room is dark enough to turn off the backlight when the LCD is in Auto mode.
*/
bool LDRBelowDarkRoomThreshold() {
  return analogRead(LDR_PIN) <= max(LDR_DARK_ROOM_THRESH - 250, 0);
}

/*
* True if the LDR is reading a value above the light room threshold (with a margin).
* This indicates the room is light enough to turn on the backlight when the LCD is in Auto mode.
*/
bool LDRAboveLightRoomThreshold() {
  return analogRead(LDR_PIN) >= min(LDR_DARK_ROOM_THRESH + 250, 4095);
}

/*
* True if the LDR is reading a value below the swipe threshold.
* Updated every minute.
*/
bool LDRBelowSwipeThreshold() {
  return analogRead(LDR_PIN) <= _ldrSwipeThreshold;
}

/*
* Continuously checks for LDR being swiped by the user and calls LDRSwiped if so.
* Debounced.
*/
void ProcessSwiping() {
  static const int delayBeforeReturn = 2500;                        // How long to remain in the ldr-swipe cycling view after no ldr swipe change before returning back to the stat display screen.
  static const int debounceDelay = 50;                              // How long the LDR must read a consistant high or low value to be considered stable input.
  static const int minSwipeDarknessTime = 150;                      // The minimum amount of time (ms) the LDR must be in dark state to consider a thumb swipe as having stated.
  static const int maxSwipeDarknessTime = 2500;                     // After this time (ms) it is unlikely to be a thumb swipe and more likely to be the lights turning off.

  static elapsedMillis debounceTimer;                               // Timer for debouncing LDR readings.
  static bool previouslyBelowLimit;                                 // LDR reading from the previous invocation. Used for debouncing, not for tracking complete state changes.

  static elapsedMillis darkTimer;                                   // Timer for tracking darkness duration.
  static bool previouslyInDarkness = false;                         // Tracks if LDR has been in stable darkness. Not to be confused with previouslyBelowLimit.

  bool ldrSwipeActioned = false;                                    // Whether the action associated with an LDR swipe occured in this invocation. If so, post-swipe action steps will occur after the "delayBeforeReturn" timout.

  while (true) {
    bool belowSwipeThreshold = LDRBelowSwipeThreshold();

    if (belowSwipeThreshold != previouslyBelowLimit) {              // Debounce the LDR input.
      DEBUG_SERIAL.print("LDR state change detected. Now: ");
      DEBUG_SERIAL.println(analogRead(LDR_PIN));
      debounceTimer = 0;
    }

    if (debounceTimer > debounceDelay) {                            // Readings have stabilized.
      if (belowSwipeThreshold && !previouslyInDarkness) {           // Moving from light to dark.
        DEBUG_SERIAL.println("Moving from light state to dark state");
        previouslyInDarkness = true;                                // Have moved into a dark state.
        darkTimer = 0;                                              // Begin tracking how long the unit has been in darkness.
      }

      if (!belowSwipeThreshold && previouslyInDarkness) {           // Moving from dark to light.
        DEBUG_SERIAL.println("Moving from dark state to light state");
        previouslyInDarkness = false;                               // Reset for next loop.
        if (darkTimer >= minSwipeDarknessTime && darkTimer <= maxSwipeDarknessTime) {
          DEBUG_SERIAL.println("Duration lasted length of thumb swipe");
          LDRSwiped();                                              // Between 150ms and 3000ms, likely a thumb swipe.
          ldrSwipeActioned = true;
        }
      }
    }

    previouslyBelowLimit = belowSwipeThreshold;                     // Update value for next loop debouncing.

    if (!ldrSwipeActioned) {                                        // If no LDR swipe was actioned, return immediately, this essentially prevents any looping from taking place during 99% of invocations.
      return;
    }

    if (ldrSwipeActioned && debounceTimer > delayBeforeReturn) {    // Post LDR swipe actions that should be performed after a timeout.
      LDRPostSwipeTimoutElapsed();
      return;
    }
  }
}

/*
* Called when a users thumb swipe across the LDR is detected.
* Thumb swipe should put the LDR into a dark state for between 200 and 3000ms.
*/
void LDRSwiped() {
  DEBUG_SERIAL.println("LDR swipe action commencing");
  _selectedValueIndex++;                          // Increment the value to be shown. Wrap around if moved out of range.
  if (_selectedValueIndex >= API_VALUE_COUNT) {
    _selectedValueIndex = 0;
  }

  DEBUG_SERIAL.print("Selected value index now: ");
  DEBUG_SERIAL.println(_selectedValueIndex);

  WriteToLCD(                                     // Display the summary for the currently selected option.
    _valueSelectionSummary[_selectedValueIndex][0],
    _valueSelectionSummary[_selectedValueIndex][1]
  );
}

/*
* Called when the LDR was swiped, the action completed and the post call timeout has elapsed.
* Configured to save the new setting to EEPROM and render the current stat.
*/
void LDRPostSwipeTimoutElapsed() {
  SaveConfigToEEPROM();
  delay(2000);
  ProcessDisplayValueUpdate(true);    // Call display update with override value to clear the button message from the screen and display the stat again.
}