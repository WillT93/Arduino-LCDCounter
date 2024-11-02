#ifndef _T93_LCD_COUNTER_BUTTONS_h
#define _T93_LCD_COUNTER_BUTTONS_h

void InitializeButtons();
void ProcessButtons();
void Button1Pressed();
void Button2Pressed();
void Button1PostPressTimeoutElapsed();
void Button2PostPressTimeoutElapsed();

#endif