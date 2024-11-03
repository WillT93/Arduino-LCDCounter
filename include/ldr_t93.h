#ifndef _T93_LCD_COUNTER_LDR_h
#define _T93_LCD_COUNTER_LDR_h

void InitializeLDR();
void ProcessLDR();
void UpdateBacklightPerLightLevel();
bool LDRBelowDarkRoomThreshold();
bool LDRAboveLightRoomThreshold();

#endif