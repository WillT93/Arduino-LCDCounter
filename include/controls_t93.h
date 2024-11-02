#ifndef _T93_LCD_COUNTER_CONTROLS_h
#define _T93_LCD_COUNTER_CONTROLS_h

void InitializeInputDevices();

void ProcessButtons();
void Button1Pressed();
void Button2Pressed();
void Button1PostPressTimeoutElapsed();
void Button2PostPressTimeoutElapsed();

void ProcessLDR();
bool CalibrateLDRThresholds();
bool LDRReadingDecrease();
bool LDRReadingIncrease();
void LDRSwiped();
void LDRPostSwipeTimoutElapsed();

#endif