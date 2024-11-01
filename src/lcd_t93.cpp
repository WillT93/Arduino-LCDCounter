#include "globals_t93.h"
#include "lcd_t93.h"

/*
* Initializes I2C comms with the LCD. Sets backlight according to users preferences.
*/
void InitializeLCD() {
  DEBUG_SERIAL.println("Initializing LCD");
  
  _lcd.init();
  if (_selectedDisplayMode == On) {
    _lcd.backlight();
    _lcdBacklightOn = true;
  }
  else if (_selectedDisplayMode == Off) {
    _lcd.noBacklight();
    _lcdBacklightOn = false;
  }
  else if (_selectedDisplayMode == Auto && analogRead(LDR_PIN < LDR_THRESHOLD)) {
    _lcd.noBacklight();
    _lcdBacklightOn = false;
  }
  else if (_selectedDisplayMode == Auto && analogRead(LDR_PIN >= LDR_THRESHOLD)) {
    _lcd.backlight();
    _lcdBacklightOn = true;
  }  
  
  // Import the custom chars into the LCD config.
  for (int i = 0; i < LEN(animationCustomChars); i++) {
    _lcd.createChar(i, animationCustomChars[i]);
  }
  
  DEBUG_SERIAL.println("LCD initialized!");
}

/*
* Writes provided text to the top and bottom rows of the LCD.
* If animation is specified, that will play prior to the update.
*/
void WriteToLCD(const char* topRow, const char* bottomRow, bool animate) {
  if (animate) {
    PerformLCDAnimation();
  }

  _lcd.clear();
  _lcd.setCursor(0, 0);
  _lcd.print(topRow);
  _lcd.setCursor(0, 1);
  _lcd.print(bottomRow);
}

/*
* Clears the LCD and performs a sine-wave animation on it. Used when the value updates to draw the users attention.
*/
void PerformLCDAnimation() {
  _lcd.clear();
  for (int i = 0; i < 3; i++) {                               // Loop the animation three times.
    for (int frame = 0; frame < ANIM_FRAME_COUNT; frame++) {  // For each frame in the animation...
      for (int column = 0; column < LCD_COLUMNS; column++) {  // For each column in the display...
        int frameIndex = column;                              // Start with the column index, this means for a frame array containing ['\', '_', '/'] the first three columns will show \_/.
        while (frameIndex >= ANIM_FRAME_COUNT) {              // There may be more columns than animation frames, if so pull it back to within the bounds of the animation frame array.
          frameIndex -= ANIM_FRAME_COUNT;                     // For a 16 column display, the frame index will now be 0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7 for each of the 16 columns respectively.
        }
        frameIndex -= frame;                                  // Shift the frame index left by the number of frames already passed in the animation.
                                                              // For frame 0: Col 3 has frameIndex 3, Col 4 has frameIndex 4 etc. For frame 1: Col 3 has frameIndex 2, Col 4 has frameIndex 3 etc.
                                                              // This means for each frame in the animation, the char displayed on a column is the one previously displayed on the column to its left, creating the illusion of rightwards movement.
        if (frameIndex < 0) {                                 // If left shifted earlier than the first frame, wrap back around to the last frame. In previous example in frame 2, Col 1 has frameIndex -1 which is wrapped around to frameIndex 7.
          frameIndex += 8;
        }
        for (int row = 0; row < LCD_ROWS; row ++) {           // Draw the correct character in the upper and lower rows for that column.
          _lcd.setCursor(column, row);
          _lcd.write(animationSequence[frameIndex][row]);
        }
    }
    delay(100);
    }
  }
  _lcd.clear();
}