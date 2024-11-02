#ifndef _T93_LCD_COUNTER_CONTROLS_h
#define _T93_LCD_COUNTER_CONTROLS_h

void InitializeInputDevices();

void ProcessButtons();
void Button1Pressed();
void Button2Pressed();

void ProcessLDR();
bool LDRReadingDarkness();
void LDRSwiped();

#endif