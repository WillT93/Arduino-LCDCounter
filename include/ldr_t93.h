#ifndef _T93_LCD_COUNTER_LDR_h
#define _T93_LCD_COUNTER_LDR_h

void InitializeLDR();
void ProcessLDR();
void UpdateBacklightPerLightLevel();
void CalibrateLDRSwipeThreshold();
bool LDRBelowDarkRoomThreshold();
bool LDRAboveLightRoomThreshold();
bool LDRBelowSwipeThreshold();
void ProcessSwiping();
void LDRSwiped();
void LDRPostSwipeTimoutElapsed();

#endif