#ifndef _T93_LCD_COUNTER_LCD_h
#define _T93_LCD_COUNTER_LCD_h

#define ANIM_FRAME_COUNT      8                 // The number of frames in the LCD animation sequence.
#define LCD_COLUMNS           16                // Number of columns in the LCD.
#define LCD_ROWS              2                 // Number of rows in the LCD.
#define LCD_ADDRESS           0x27              // The I2C address the LCD lives at. Can be found using an I2C scanning sketch.
#define LDR_THRESHOLD         100               // The threshold the LDR must be below in order for it to be considered in "darkness".

void InitializeLCD();
void WriteToLCD(const char*, const char* = "", bool = false);
void PerformLCDAnimation();

#endif