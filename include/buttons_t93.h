#ifndef _T93_LCD_COUNTER_BUTTONS_h
#define _T93_LCD_COUNTER_BUTTONS_h

#include "enums_t93.h"

void InitializeButtons();

void ProcessButtons();

ButtonActionResult CheckButtonOneState();
void ButtonOneQuickPressed();
void ButtonOneHoldPressed();
void ButtonOnePostQuickPressRelease();
void ButtonOnePostHoldPressRelease();

ButtonActionResult CheckButtonTwoState();
void ButtonTwoQuickPressed();
void ButtonTwoHoldPressed();
void ButtonTwoPostQuickPressRelease();
void ButtonTwoPostHoldPressRelease();

#endif