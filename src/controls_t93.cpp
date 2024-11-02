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
  DEBUG_SERIAL.println("Controls initialized");
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
    DEBUG_SERIAL.println("Button 1 state change detected");
    btn1LastChanged = 0;
  }
  if (btn1LastChanged >= debounceDelay && currentBtn1State == HIGH) {
    DEBUG_SERIAL.println("Button 1 debounce passed");
    Button1Pressed();
    btn1LastChanged = 0;
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
    btn2LastChanged = 0;
  }
  lastBtn2State = currentBtn2State;
}

/*
* Called when button 1 is pressed for at least 50 ms.
* Currently configured to cycle through the various LDR based LCD dimming modes.
*/
void Button1Pressed() {
  DEBUG_SERIAL.println("Button 1 action commencing");
  if (_selectedDisplayMode == On) {
    DEBUG_SERIAL.println("Setting display backlight to Always Off");
    WriteToLCD("LCD backlight", "always off");
    delay(2000);
    _selectedDisplayMode = Off;
    _lcd.noBacklight();
    _lcdBacklightOn = false;
  }

  else if (_selectedDisplayMode == Off) {
    DEBUG_SERIAL.println("Setting display backlight to Auto");
    WriteToLCD("LCD backlight", "Auto (light dep)");
    delay(2000);
    _selectedDisplayMode = Auto;
    if (analogRead(LDR_PIN < LDR_THRESHOLD)) {
      DEBUG_SERIAL.print("Turning backlight off based on LDR level of ");
      DEBUG_SERIAL.println(analogRead(LDR_PIN));
      _lcd.noBacklight();
      _lcdBacklightOn = false;
    }
    else {
      DEBUG_SERIAL.print("Turning backlight on based on LDR level of ");
      DEBUG_SERIAL.println(analogRead(LDR_PIN));
      _lcd.backlight();
      _lcdBacklightOn = true;
    }  
  }

  else if (_selectedDisplayMode == Auto) {
    DEBUG_SERIAL.println("Setting display backlight to Always On");
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
  DEBUG_SERIAL.println("Button 1 action commencing");
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
    DEBUG_SERIAL.println("LDR state change detected");
    debounceTimer = 0;
  }

  if (debounceTimer > debounceDelay) {                            // Readings have stabilized.
    DEBUG_SERIAL.println("LDR debounce passed");
    if (readingDarkness && !previouslyInDarkness) {               // Moving from light to dark.
      DEBUG_SERIAL.println("Moving from light state to dark state");
      previouslyInDarkness = true;                                // Have moved into a dark state.
      darkTimer = 0;                                              // Begin tracking how long the unit has been in darkness.
    }

    if (readingDarkness && previouslyInDarkness) {                // Currently in darkness, and have been for a while.
      DEBUG_SERIAL.println("Remaining in dark state from previously dark state");
      if (darkTimer > maxSwipeDarknessTime && _lcdBacklightOn) {  // Have been in darkness long enough to turn off backlight.
        DEBUG_SERIAL.println("Duration has passed to consider the room dark");
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
        LDRSwiped();                                              // Between 200ms and 3000ms, likely a thumb swipe.
      }
      else if (darkTimer > maxSwipeDarknessTime) {                // Was in darkness for more than 3000ms, likely lights were off and now are back on.
        DEBUG_SERIAL.println("was in dark state long enough to consider the room previously being dark");
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
  SaveConfigToEEPROM();

  WriteToLCD(                                     // Display the summary for the currently selected option.
    _valueSelectionSummary[_selectedValueIndex][0],
    _valueSelectionSummary[_selectedValueIndex][1]
  );
}