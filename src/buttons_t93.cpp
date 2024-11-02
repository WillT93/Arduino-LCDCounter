#include <Arduino.h>
#include <elapsedMillis.h>

#include "globals_t93.h"
#include "secrets_t93.h"
#include "lcd_t93.h"
#include "ldr_t93.h"
#include "eeprom_t93.h"
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