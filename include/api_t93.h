#ifndef _T93_LCD_COUNTER_API_h
#define _T93_LCD_COUNTER_API_h

#include <HTTPClient.h>

void ProcessAPIPolling();
void UpdateValueFromAPI();
void ReadResponseStream(HTTPClient&, char*, int);
void RemoveAsteriskNotation(char*);
bool ValidatePayloadFormat(char*);

#endif