#include <Arduino.h>
#include <elapsedMillis.h>

#include "globals_t93.h"
#include "secrets_t93.h"
#include "lcd_t93.h"
#include "ldr_t93.h"
#include "eeprom_t93.h"
#include "enums_t93.h"
#include "buttons_t93.h"

/*
* Configures pinMode etc for the various button pins.
*/
void InitializeButtons() {
  DEBUG_SERIAL.println("Initializing buttons");
  pinMode(BTN_1_PIN, INPUT);
  pinMode(BTN_2_PIN, INPUT);

  DEBUG_SERIAL.println("Buttons initialized");
}

void ProcessButtons() {
  static const int delayBeforeReturn = 3000;    // How long to remain in the button cycling view after no button change before returning back to the caller.

  bool buttonOneQuickPressed = false;           // Whether button one was quick pressed during this invocation. If so, loop won't return immediately, but wait for the delay and button one quick press final actions.
  bool buttonOneHoldPressed = false;
  bool buttonTwoQuickPressed = false;
  bool buttonTwoHoldPressed = false;

  bool buttonOnePostQuickPressComplete = false; // Whether the quickpress actions associated with button 1 have been completed. If button one was quick pressed and this is not true, the loop will wait until it is.
  bool buttonOnePostHoldPressComplete = false;
  bool buttonTwoPostQuickPressComplete = false;
  bool buttonTwoPostHoldPressComplete = false;

  elapsedMillis buttonOneLastChanged;           // The time since button one was released, once larger than delayBeforeReturn, the post-action events will be triggered and return to caller.
  elapsedMillis buttonTwoLastChanged;

  // The while loop seems unintuitive, however, it makes for a better UI.
  // Most of the time, the while loop will exit during the first iteration, as an if-statement exists where (if no button action was performed) it just returns.
  // When a button action is performed, it doesn't make sense to return to the called immediately, usually there is some feedback to display for a while on the LCD or the unit should wait to see if there are additional button presses.
  // Once a sufficient time has passed with no further input, another if-statement kicks in which performs a closing action, and returns to caller. 
  while (true) {
    ButtonActionResult buttonOneReading = CheckButtonOneState();
    ButtonActionResult buttonTwoReading = CheckButtonTwoState();

    if (buttonOneReading == QuickPress) {
      ButtonOneQuickPressed();
      buttonOneQuickPressed = true;
      buttonOneLastChanged = 0;
    }
    else if (buttonOneReading == HoldPress) {
      ButtonOneHoldPressed();
      buttonOneHoldPressed = true;
      buttonOneLastChanged = 0;
    }
    else if (buttonTwoReading == QuickPress) {
      ButtonTwoQuickPressed();
      buttonTwoQuickPressed = true;
      buttonTwoLastChanged = 0;
    }
    else if (buttonTwoReading == HoldPress) {
      ButtonTwoHoldPressed();
      buttonTwoHoldPressed = true;
      buttonTwoLastChanged = 0;
    }

    if (buttonOneQuickPressed && buttonOneLastChanged > delayBeforeReturn) {  // If button one was quick pressed and sufficient time has passed...
      ButtonOnePostQuickPressRelease();                                       // Perform any post quick press actions and
      buttonOnePostQuickPressComplete = true;                                 // Mark actions complete (all post-press actions must be complete for any actioned button in order for loop to exit)
    }

    if (buttonOneHoldPressed && buttonOneLastChanged > delayBeforeReturn) {
      ButtonOnePostHoldPressRelease();
      buttonOnePostHoldPressComplete = true;
    }

    if (buttonTwoQuickPressed && buttonTwoLastChanged > delayBeforeReturn) {
      ButtonTwoPostQuickPressRelease();
      buttonTwoPostQuickPressComplete = true;
    }

    if (buttonTwoHoldPressed && buttonTwoLastChanged > delayBeforeReturn) {
      ButtonTwoPostHoldPressRelease();
      buttonTwoPostHoldPressComplete = true;
    }

    if (                                                                  // If no button was actioned, OR a button was actioned and all post actions have been performed for any button that had activity, return to caller.
      (!buttonOneQuickPressed || (buttonOneQuickPressed && buttonOnePostQuickPressComplete)) &&
      (!buttonOneHoldPressed || (buttonOneHoldPressed && buttonOnePostHoldPressComplete)) &&
      (!buttonTwoQuickPressed || (buttonTwoQuickPressed && buttonTwoPostQuickPressComplete)) &&
      (!buttonTwoHoldPressed || (buttonTwoHoldPressed && buttonTwoPostHoldPressComplete))
    ) {
      return;
    }
  }
}

/*
* Non-blocking check on the status of a button.
* Checks for button push, immediate release or release after hold and calls appropriate action.
* Returns an enum indicating what action, if any, took place.
*/
ButtonActionResult CheckButtonOneState() {
  static const int debounceDelay = 50;                                    // How long the button pin must read (in milliseconds) without value fluctuation to be considered "input".
  static elapsedMillis debounceTimer = 0;                                 // How long ago a state change was detected on the button, reset with each change. Enables debouncing of input.
  static bool previousState = LOW;                                        // The button state on the previous loop. If this differs from the state in this loop, debounceTimer gets reset.

  static const int minimumTimeForHold = 750;                              // How long the button must read HIGH to be considered held in as opposed to just pushed.
  static elapsedMillis buttonHighTimer = 0;                               // Tracks how long the button has been in HIGH state for. Used to distinguish between press and press-and-hold.
  static bool buttonHigh = false;                                         // Set to high when button pushed, used to track when button releases, rather than just button remaining unpressed.
  ButtonActionResult result = None;                                       // The action that occured during this loop.

  bool currentButtonState = digitalRead(BTN_1_PIN);                       // Debounce button. Each change in state vs the previous invocation restarts the timer.
  if (currentButtonState != previousState) {
    DEBUG_SERIAL.println("Button 1 state change detected");
    debounceTimer = 0;
  }

  if (debounceTimer >= debounceDelay && currentButtonState == HIGH) {     // Button has passed the debounce check and is being pressed. Begin the timer to determine how long it's been held for.
    DEBUG_SERIAL.println("Button 1 stable HIGH");
    if (!buttonHigh) {
      buttonHighTimer = 0;                                                // Don't want to reset the timer the whole time the button is being held, only the first time it moves from unpressed to pressed.
      buttonHigh = true;
    }
    result = Push;
  }

  if (                                                                    // BUtton has been release. Evaluate whether it was a quick press or a press-and hold. Return as such.
    debounceTimer >= debounceDelay &&
    currentButtonState == LOW &&
    buttonHigh
  ) {
    DEBUG_SERIAL.println("Button 1 stable LOW");

    if (buttonHighTimer >= minimumTimeForHold) {                          // If the button was high long enough to consider it held, return that information.
      DEBUG_SERIAL.println("Button 1 held");
      buttonHigh = false;
      result = HoldPress;
    }
    else {                                                                // Otherwise the button was only pressed briefly, return that.
      DEBUG_SERIAL.println("Button 1 briefly pressed");
      buttonHigh = false;
      result = QuickPress;
    }
  }

  previousState = currentButtonState;                                     // Update the variable to allow the debouncing to compare this loops state in the next one.
  return result;
}

/*
* Non-blocking check on the status of a button.
* Checks for button push, immediate release or release after hold and calls appropriate action.
* Returns an enum indicating what action, if any, took place.
*/
ButtonActionResult CheckButtonTwoState() {
  static const int debounceDelay = 50;                                    // How long the button pin must read (in milliseconds) without value fluctuation to be considered "input".
  static elapsedMillis debounceTimer = 0;                                 // How long ago a state change was detected on the button, reset with each change. Enables debouncing of input.
  static bool previousState = LOW;                                        // The button state on the previous loop. If this differs from the state in this loop, debounceTimer gets reset.

  static const int minimumTimeForHold = 750;                              // How long the button must read HIGH to be considered held in as opposed to just pushed.
  static elapsedMillis buttonHighTimer = 0;                               // Tracks how long the button has been in HIGH state for. Used to distinguish between press and press-and-hold.
  static bool buttonHigh = false;                                         // Set to high when button pushed, used to track when button releases, rather than just button remaining unpressed.
  ButtonActionResult result = None;                                       // The action that occured during this loop.

  bool currentButtonState = digitalRead(BTN_2_PIN);                       // Debounce button. Each change in state vs the previous invocation restarts the timer.
  if (currentButtonState != previousState) {
    DEBUG_SERIAL.println("Button 2 state change detected");
    debounceTimer = 0;
  }

  if (debounceTimer >= debounceDelay && currentButtonState == HIGH) {     // Button has passed the debounce check and is being pressed. Begin the timer to determine how long it's been held for.
    DEBUG_SERIAL.println("Button 2 stable HIGH");
    if (!buttonHigh) {
      buttonHighTimer = 0;                                                // Don't want to reset the timer the whole time the button is being held, only the first time it moves from unpressed to pressed.
      buttonHigh = true;
    }
    result = Push;
  }

  if (                                                                    // BUtton has been release. Evaluate whether it was a quick press or a press-and hold. Return as such.
    debounceTimer >= debounceDelay &&
    currentButtonState == LOW &&
    buttonHigh
  ) {
    DEBUG_SERIAL.println("Button 2 stable LOW");

    if (buttonHighTimer >= minimumTimeForHold) {                          // If the button was high long enough to consider it held, return that information.
      DEBUG_SERIAL.println("Button 2 held");
      buttonHigh = false;
      result = HoldPress;
    }
    else {                                                                // Otherwise the button was only pressed briefly, return that.
      DEBUG_SERIAL.println("Button 2 briefly pressed");
      buttonHigh = false;
      result = QuickPress;
    }
  }

  previousState = currentButtonState;                                     // Update the variable to allow the debouncing to compare this loops state in the next one.
  return result;
}

/*
* Called when button 1 is pressed for at least 50 ms but no more than 2000 ms.
* Currently configured to cycle through the various stats.
*/
void ButtonOneQuickPressed() {
  DEBUG_SERIAL.println("Button 1 quick release action commencing");
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

  delay(100); // Prevents skipping over options.
}

/*
* Called when button 1 is pressed for at least 2000 ms.
* Currently configured to cycle through the various LDR based LCD dimming modes.
*/
void ButtonOneHoldPressed() {
  DEBUG_SERIAL.println("Button 1 held release action commencing");
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
    if (LDRBelowDarkRoomThreshold()) {
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
* Called when button 1 was quick pressed, the action completed, and the timeout elapsed.
* Configured to save values to EEPROM and re-render the stat screen.
*/
void ButtonOnePostQuickPressRelease() {
  DEBUG_SERIAL.println("Button 1 post quick release action commencing");
  SaveConfigToEEPROM();
  ProcessDisplayValueUpdate(true);    // Call display update with override value to clear the button message from the screen and display the stat again.
}

/*
* Called when button 1 was long pressed, the action completed, and the timeout elapsed.
* Configured to action the LCD backlight configuration, then save values to EEPROM and re-render the stat screen.
*/
void ButtonOnePostHoldPressRelease() {
  DEBUG_SERIAL.println("Button 1 post held release action commencing");
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
* Called when button 2 is pressed for at least 50 ms but no more than 2000 ms.
*/
void ButtonTwoQuickPressed() {
  DEBUG_SERIAL.println("Button 2 quick release action commencing");
  // Currently this does nothing. Provision for future.
}

/*
* Called when button 2 is pressed for at least 2000 ms.
*/
void ButtonTwoHoldPressed() {
  DEBUG_SERIAL.println("Button 2 held release action commencing");
  // Currently this does nothing. Provision for future.
}

/*
* Called when button 2 was pressed briefly, the action completed, and the timeout elapsed.
*/
void ButtonTwoPostQuickPressRelease() {
  DEBUG_SERIAL.println("Button 2 post quick release action commencing");
  // Currently this does nothing. Provision for future.
}

/*
* Called when button 2 was pressed and held, the action completed, and the timeout elapsed.
*/
void ButtonTwoPostHoldPressRelease() {
  DEBUG_SERIAL.println("Button 2 post held release action commencing");
  // Currently this does nothing. Provision for future.
}