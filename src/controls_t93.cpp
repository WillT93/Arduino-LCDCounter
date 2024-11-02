#include <Arduino.h>
#include <elapsedMillis.h>

#include "globals_t93.h"
#include "secrets_t93.h"
#include "lcd_t93.h"
#include "eeprom_t93.h"
#include "controls_t93.h"

/*
* Configures pinMode etc for the various button and LDR pins.
*/
void InitializeInputDevices() {
  DEBUG_SERIAL.println("Initializing controls");
  pinMode(BTN_1_PIN, INPUT);
  pinMode(BTN_2_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);

  CalibrateLDRThresholds();

  DEBUG_SERIAL.println("Controls initialized");
}

/*
* Non-blocking check on the status of the buttons.
* Checks if either button is being pushed or released and calls associated function.
*/
void ProcessButtons() {
  static const int delayBeforeReturn = 2500;                      // How long to remain in the button cycling view after no button change before returning back to the stat display screen.
  static const int debounceDelay = 50;                            // How long the button pin must read (in milliseconds) without value fluctuation to be considered "input".

  static elapsedMillis btn1LastChanged = 10000;                   // Init all three to a "long ago" value. Statics so this will only be the value on first boot.
  static elapsedMillis btn2LastChanged = 10000;
  
  static bool lastBtn1State = LOW;                                // Statics, will retain value between method invocations.
  static bool lastBtn2State = LOW;

  bool btn1Actioned = false;                                      // Whether the action associated with button 1 occured in this invocation. If so, post-button action steps will occur after the "delayBeforeReturn" timout.
  bool btn2Actioned = false;
  
  while (true) {                                                  // Blocking loop that prevents return to caller (and thus UI freezing due to API polling) within it button state is evaluated (debounced). Specific conditions allow for returns.
    // Button 1 processing
    bool currentBtn1State = (digitalRead(BTN_1_PIN));
    if (currentBtn1State != lastBtn1State) {
      DEBUG_SERIAL.println("Button 1 state change detected");
      btn1LastChanged = 0;
    }
    if (btn1LastChanged >= debounceDelay && currentBtn1State == HIGH) {
      DEBUG_SERIAL.println("Button 1 debounce passed");
      Button1Pressed();
      btn1Actioned = true;
    }
    lastBtn1State = currentBtn1State;

    // Button 2 processing
    bool currentBtn2State = (digitalRead(BTN_2_PIN));
    if (currentBtn2State != lastBtn2State) {
      DEBUG_SERIAL.println("Button 2 state change detected");
      btn2LastChanged = 0;
    }
    if (btn2LastChanged >= debounceDelay && currentBtn2State == HIGH) {
      DEBUG_SERIAL.println("Button 2 debounce passed");
      Button2Pressed();
      btn2Actioned = true;
    }
    lastBtn2State = currentBtn2State;

    if (!btn1Actioned && !btn2Actioned) {                         // If no button was pressed, return immediately, this essentially prevents any looping from taking place during 99% of invocations.
      return;
    }

    if (btn1Actioned && btn1LastChanged > delayBeforeReturn) {    // Post-button 1 actions that should be performed after a timeout.
      Button1PostPressTimeoutElapsed();
      return;
    }

    if (btn2Actioned && btn2LastChanged > delayBeforeReturn) {    // Post-button 2 actions that should be performed after a timeout.
      Button2PostPressTimeoutElapsed();
      return;
    }
  }
}

/*
* Called when button 1 is pressed for at least 50 ms.
* Currently configured to cycle through the various LDR based LCD dimming modes.
*/
void Button1Pressed() {
  DEBUG_SERIAL.println("Button 1 action commencing");
  if (_selectedDisplayMode == On) {
    _lcd.backlight();
    DEBUG_SERIAL.println("Setting display backlight to Always Off");
    WriteToLCD("LCD backlight", "always off");
    _selectedDisplayMode = Off;
    _lcdBacklightOn = false;
  }

  else if (_selectedDisplayMode == Off) {
    _lcd.backlight();
    DEBUG_SERIAL.println("Setting display backlight to Auto");
    WriteToLCD("LCD backlight", "Auto (light dep)");
    _selectedDisplayMode = Auto;
    if (LDRReadingDecrease()) {
      DEBUG_SERIAL.print("Turning backlight off based on LDR level of ");
      DEBUG_SERIAL.println(analogRead(LDR_PIN));
      _lcdBacklightOn = false;
    }
    else {
      DEBUG_SERIAL.print("Turning backlight on based on LDR level of ");
      DEBUG_SERIAL.println(analogRead(LDR_PIN));
      _lcdBacklightOn = true;
    }  
  }

  else if (_selectedDisplayMode == Auto) {
    _lcd.backlight();
    DEBUG_SERIAL.println("Setting display backlight to Always On");
    WriteToLCD("LCD backlight", "always on");
    _selectedDisplayMode = On;
    _lcdBacklightOn = true;
  }

  delay(100); // Prevents skipping over options.
}

/*
* Called when button 1 was pressed, the action completed, and the timeout elapsed.
* Configured to action the LCD backlight configuration, then save values to EEPROM and re-render the stat screen.
*/
void Button1PostPressTimeoutElapsed() {
  if (_lcdBacklightOn) {
    _lcd.backlight();
  }
  else {
    _lcd.noBacklight();
  }
  SaveConfigToEEPROM();
  ProcessDisplayValueUpdate(true);    // Call display update with override value to clear the button message from the screen and display the stat again.
}

/*
* Called when button 2 is pressed for at least 50ms.
*/
void Button2Pressed() {
  DEBUG_SERIAL.println("Button 2 action commencing");
  // Currently this does nothing. Provision for future.
}

/*
* Called when button 2 was pressed, the action completed, and the timeout elapsed.
*/
void Button2PostPressTimeoutElapsed() {
  DEBUG_SERIAL.println("Button 2 action commencing");
  // Currently this does nothing. Provision for future.
}

/*
* Non-blocking check on the status of the LDR.
* Used to check whether the user is swiping or has swiped across the sensor, as well as the current level of room brightness.
* If user swipe is detected, LDRSwiped() is called. Otherwise if the room is in a dark state, the backlight will be turned off if configured to do so.
*/
void ProcessLDR() {
  static const int delayBeforeReturn = 2500;                        // How long to remain in the ldr-swipe cycling view after no ldr swipe change before returning back to the stat display screen.
  static const int debounceDelay = 50;                              // How long the LDR must read a consistant high or low value to be considered stable input.
  static const int minSwipeDarknessTime = 150;                      // The minimum amount of time (ms) the LDR must be in dark state to consider a thumb swipe as having stated.
  static const int maxSwipeDarknessTime = 2500;                     // After this time (ms) it is unlikely to be a thumb swipe and more likely to be the lights turning off.
  static const int recalibrationInterval = 60000;                   // The interval with which the LDR will recalibrate it's value. This will also happen when it detects the lights turning on and off.

  static elapsedMillis debounceTimer;                               // Timer for debouncing LDR readings.
  static bool previouslyReadingDarkness;                            // LDR reading from the previous invocation. Used for debouncing, not for tracking complete state changes.

  static elapsedMillis darkTimer;                                   // Timer for tracking darkness duration.
  static bool previouslyInDarkness = false;                         // Tracks if LDR has been in stable darkness. Not to be confused with previouslyReadingDarkness.

  static elapsedMillis recalibrationTimer = 0;                      // Timer to track recalibration runs.

  bool ldrSwipeActioned = false;                                    // Whether the action associated with an LDR swipe occured in this invocation. If so, post-swipe action steps will occur after the "delayBeforeReturn" timout.

  if (recalibrationTimer > recalibrationInterval) {
    CalibrateLDRThresholds();
  }

  while (true) {
    bool readingDarkness = LDRReadingDecrease();

    if (readingDarkness != previouslyReadingDarkness) {             // Debounce the LDR input.
      DEBUG_SERIAL.print("LDR state change detected. Now: ");
      DEBUG_SERIAL.println(analogRead(LDR_PIN));
      debounceTimer = 0;
    }

    if (debounceTimer > debounceDelay) {                            // Readings have stabilized.
      if (readingDarkness && !previouslyInDarkness) {               // Moving from light to dark.
        DEBUG_SERIAL.println("Moving from light state to dark state");
        previouslyInDarkness = true;                                // Have moved into a dark state.
        darkTimer = 0;                                              // Begin tracking how long the unit has been in darkness.
      }

      if (readingDarkness && previouslyInDarkness) {                // Currently in darkness, and have been for a while.
        DEBUG_SERIAL.println("Remaining in dark state from previously dark state");
        if (darkTimer > maxSwipeDarknessTime && _lcdBacklightOn) {  // Have been in darkness long enough to turn off backlight.
          DEBUG_SERIAL.println("Was in dark state long enough to consider the room moving into dark state");
          CalibrateLDRThresholds();
          if (_selectedDisplayMode == Auto) {                       // Respect user choice.
            DEBUG_SERIAL.println("LCD in auto mode, turning off backlight");
            _lcd.noBacklight();
            _lcdBacklightOn = false;
          }
          else {
            DEBUG_SERIAL.println("LCD in manual mode, no change to backlight needed");
          }
        }
      }

      if (!readingDarkness && previouslyInDarkness) {               // Moving from dark to light.
        DEBUG_SERIAL.println("Moving from light state to dark state");
        previouslyInDarkness = false;                               // Reset for next loop.
        if (darkTimer >= minSwipeDarknessTime && darkTimer <= maxSwipeDarknessTime) {
          DEBUG_SERIAL.println("Duration lasted length of thumb swipe");
          LDRSwiped();                                              // Between 150ms and 3000ms, likely a thumb swipe.
          ldrSwipeActioned = true;
        }
        else if (darkTimer > maxSwipeDarknessTime) {                // Was in darkness for more than 3000ms, likely lights were off and now are back on.
          DEBUG_SERIAL.println("Was in dark state long enough to consider the room previously being dark");
          CalibrateLDRThresholds();
          if (_selectedDisplayMode == Auto) {                       // Respect user choice.
            DEBUG_SERIAL.println("LCD in auto mode, turning on backlight");
            _lcd.backlight();
            _lcdBacklightOn = true;
          }
          else {
            DEBUG_SERIAL.println("LCD in manual mode, no change to backlight needed");
          }
        }
      }
    }

    previouslyReadingDarkness = readingDarkness;                    // Update value for next loop debouncing.

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
* Updated the upper and lower LDR thresholds based on a percentage increase or decrease from the currently measured values.
* Percentage increase or decrease must be at least 100pts, otherwise 100pts is used as threshold delta.
*/
bool CalibrateLDRThresholds() {
  int currentValue = analogRead(LDR_PIN);
  
  _ldrLowerThreshold = max(0, min((int)(currentValue * 0.75), currentValue - 100));     // Set to the lesser of 15% below it's current value or 100pts below, not going any lower than 0.
  _ldrUpperThreshold = min(4095, max((int)(currentValue * 1.15), currentValue + 100));  // Set to the lesser of 15% above it's current value or 100pts above, not going any higher than 4095.
}

/*
* True if the LDR is reading a significant drop in brightness over the nominal value.
* Nominal value updated every minute.
*/
bool LDRReadingDecrease() {
  return analogRead(LDR_PIN) <= _ldrLowerThreshold;
}

/*
* True if the LDR is reading a significant increase in brightness over the nominal value.
* Nominal value updated every minute.
*/
bool LDRReadingIncrease() {
  return analogRead(LDR_PIN) >= _ldrUpperThreshold;
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