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
  pinMode(BTN_1_PIN, INPUT);
  pinMode(BTN_2_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
}

/*
* Non-blocking check on the status of the buttons.
* Checks if either button is being pushed or released and calls associated function.
*/
void ProcessButtons() {
  static const int debounceDelay = 50;             // How long the button pin must read (in milliseconds) without value fluctuation to be considered "input".

  static elapsedMillis btn1LastChanged = 10000;   // Init all three to a "long ago" value. Statics.
  static elapsedMillis btn2LastChanged = 10000;
  
  static bool lastBtn1State = LOW;                // Statics, will retain value between method invocations.
  static bool lastBtn2State = LOW;
  
  // Button 1 processing
  bool currentBtn1State = (digitalRead(BTN_1_PIN));
  if (currentBtn1State != lastBtn1State) {
    btn1LastChanged = 0;
  }
  if (btn1LastChanged >= debounceDelay && currentBtn1State == HIGH) {
    Button1Pressed();
    btn1LastChanged = 0;
  }
  lastBtn1State = currentBtn1State;

  // Button 2 processing
  bool currentBtn2State = (digitalRead(BTN_2_PIN));
  if (currentBtn2State != lastBtn2State) {
    btn2LastChanged = 0;
  }
  if (btn2LastChanged >= debounceDelay && currentBtn2State == HIGH) {
    Button2Pressed();
    btn2LastChanged = 0;
  }
  lastBtn2State = currentBtn2State;
}

/*
* Called when button 1 is pressed for at least 50 ms.
* Currently configured to cycle through the various LDR based LCD dimming modes.
*/
void Button1Pressed() {
  if (_selectedDisplayMode == On) {
    WriteToLCD("LCD backlight", "always off");
    delay(2000);
    _selectedDisplayMode = Off;
    _lcd.noBacklight();
    _lcdBacklightOn = false;
  }

  else if (_selectedDisplayMode == Off) {
    WriteToLCD("LCD backlight", "Auto (light dep)");
    delay(2000);
    _selectedDisplayMode = Auto;
    if (analogRead(LDR_PIN < LDR_THRESHOLD)) {
      _lcd.noBacklight();
      _lcdBacklightOn = false;
    }
    else {
      _lcd.backlight();
      _lcdBacklightOn = true;
    }  
  }

  else if (_selectedDisplayMode == Auto) {
    WriteToLCD("LCD backlight", "always on");
    delay(2000);
    _selectedDisplayMode = On;
    _lcd.backlight();
    _lcdBacklightOn = true;
  }

  SaveConfigToEEPROM();
}

/*
* Called when button 2 is pressed for at least 50ms.
*/
void Button2Pressed() {
  // Currently this does nothing. Provision for future.
}

/*
* Non-blocking check on the status of the LDR.
* Used to check whether the user is swiping or has swiped across the sensor, as well as the current level of room brightness.
* If user swipe is detected, LDRSwiped() is called. Otherwise if the room is in a dark state, the backlight will be turned off if configured to do so.
*/
void ProcessLDR() {
  static const int debounceDelay = 50;                            // How long the LDR must read a consistant high or low value to be considered stable input.
  static const int minSwipeDarknessTime = 200;                    // The minimum amount of time (ms) the LDR must be in dark state to consider a thumb swipe as having stated.
  static const int maxSwipeDarknessTime = 3000;                   // After this time (ms) it is unlikely to be a thumb swipe and more likely to be the lights turning off.

  static elapsedMillis debounceTimer;                             // Timer for debouncing LDR readings.
  static bool previouslyReadingDarkness;                          // LDR reading from the previous invocation. Used for debouncing, not for tracking complete state changes.

  static elapsedMillis darkTimer;                                 // Timer for tracking darkness duration.
  static bool previouslyInDarkness = false;                       // Tracks if LDR has been in stable darkness. Not to be confused with previouslyReadingDarkness.

  bool readingDarkness = analogRead(LDR_PIN) < LDR_THRESHOLD;
  
  if (readingDarkness != previouslyReadingDarkness) {             // Debounce the LDR input.
    debounceTimer = 0;
  }

  if (debounceTimer > debounceDelay) {                            // Readings have stabilized.
    if (readingDarkness && !previouslyInDarkness) {               // Moving from light to dark.
      previouslyInDarkness = true;                                // We have moved into a dark state.
      darkTimer = 0;                                              // Begin tracking how long we have been in darkness.
    }

    if (readingDarkness && previouslyInDarkness) {                // We are currently in darkness, and have been for a while.
      if (darkTimer > maxSwipeDarknessTime && _lcdBacklightOn) {  // Have been in darkness long enough to turn off backlight.
        if (_selectedDisplayMode == Auto) {                       // Respect user choice.
          _lcd.noBacklight();
          _lcdBacklightOn = false;
        }
      }
    }

    if (!readingDarkness && previouslyInDarkness) {               // Moving from dark to light.
      previouslyInDarkness = false;                               // Reset for next loop.
      if (darkTimer >= minSwipeDarknessTime && darkTimer <= maxSwipeDarknessTime) {
        LDRSwiped();                                              // Between 200ms and 3000ms, likely a thumb swipe.
      }
      else if (darkTimer > maxSwipeDarknessTime) {                // Was in darkness for more than 3000ms, likely lights were off and now are back on.
        if (_selectedDisplayMode == Auto) {                       // Respect user choice.
          _lcd.backlight();
          _lcdBacklightOn = true;
        }
      }
    }
  }

  previouslyReadingDarkness = readingDarkness;                    // Update value for next loop debouncing.
}

/*
* Called when a users thumb swipe across the LDR is detected.
* Thumb swipe should put the LDR into a dark state for between 200 and 3000ms.
*/
void LDRSwiped() {
  _selectedValueIndex++;                          // Increment the value to be shown. Wrap around if we move out of range.
  if (_selectedValueIndex >= API_VALUE_COUNT) {
    _selectedValueIndex = 0;
    SaveConfigToEEPROM();
  }

  WriteToLCD(                                     // Display the summary for the currently selected option.
    _valueSelectionSummary[_selectedValueIndex][0],
    _valueSelectionSummary[_selectedValueIndex][1]
  );
}