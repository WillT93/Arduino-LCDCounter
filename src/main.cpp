#include <WiFi.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

#define LEN(arr) ((int) (sizeof (arr) / sizeof (arr)[0]))

hd44780_I2Cexp lcd(0x27, 16, 2);

int topRowAnimationSequence[] = { 5, 5, 3, 4, 1, 2, 5, 5, 5, 5, 3, 4, 1, 2, 5, 5, 5, 5, 3, 4, 1, 2, 5, 5, 5, 5, 3, 4, 1, 2, 5, 5, };
int bottomRowAnimationSequence[] = { 3, 4, 0, 0, 0, 0, 1, 2, 3, 4, 0, 0, 0, 0, 1, 2, 3, 4, 0, 0, 0, 0, 1, 2, 3, 4, 0, 0, 0, 0, 1, 2, };

const byte animationCustomChars[][8] =
{
  {
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B
  },
  {
    0x18,
    0x18,
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B
  },
  {
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x18,
    0x1B,
    0x1B
  },
  {
    0x00,
    0x00,
    0x00,
    0x00,
    0x03,
    0x03,
    0x1B,
    0x1B
  },
  {
    0x03,
    0x03,
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B,
    0x1B
  },
  {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00
  }
};

void setup() {
  Serial.begin(9600);
   // lcd.begin();
  lcd.init();
  lcd.backlight();

  // Import the custom chars into the LCD config.
  for (int i = 0; i < LEN(animationCustomChars); i++) {
    lcd.createChar(i, animationCustomChars[i]);
  }

  lcd.clear();
 }

void loop() { 
  for (int z = 0; z < 8; z++) { // For each frame in the animation (8 frames, enough time for each square to show each of the 8 animation frames)
    // i = z;
    for (int k = 0; k < 16; k++) { // For each square...
      int frameIndex = k;
      while (frameIndex >= 8) {
        frameIndex -= 8;
      }
      // frameIndex is now 0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7
      frameIndex -= z;
      if (frameIndex < 0) {
        frameIndex += 8;
      }
      lcd.setCursor(k, 0);
      lcd.write(topRowAnimationSequence[frameIndex]);
      lcd.setCursor(k, 1);
      lcd.write(bottomRowAnimationSequence[frameIndex]);
      // i++;
      // delay(1000);
   }
   delay(100);
   lcd.clear(); 
  }
}
