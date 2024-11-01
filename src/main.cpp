// TODO: LDR display blanking. (needs testing)
// TODO: LDR value selection.  (needs testing)
// TODO: Configuration cycling for different LDR modes. (Needs testing)
// TODO: Run packet analysis.
// TODO: More serial debug logs.
// TODO: More inline comments.
// TODO: break into smaller .cpp and .h.
// TODO: look into warnings about array bounds

#include <Arduino.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <EEPROM.h>
#include <elapsedMillis.h>

#include "api_t93.h"
#include "controls_t93.h"
#include "eeprom_t93.h"
#include "enums_t93.h"
#include "globals_t93.h"
#include "lcd_t93.h"
#include "secrets_t93.h"
#include "wifi_t93.h"

// Debugging configuration
#define DEBUG                 true              // Optional printing of debug messages to serial.
#define EEPROM_INIT           false             // When set to true, EEPROM is written over with 0's. Perform once per ESP32 unit.

void setup() {
  if (EEPROM_INIT) {
    InitializeEEPROM();
    return;
  }

  DEBUG_SERIAL.begin(9600);

  LoadConfigFromEEPROM();
  InitializeInputDevices();
  InitializeLCD();
  InitializeWiFi();
}

void loop() {
  ProcessAPIPolling();
  ProcessButtons();
  ProcessLDR();
}

#pragma region FunctionDefinitions
/*
* Non-blocking check on whether the API needs polling.
* When the polling interval has been reached, the API will be contacted for a value update.
*/
void ProcessAPIPolling() {
  static elapsedMillis _apiPollTimer = 10000; // Initialize to a value ready for polling. Static so will retain any updated values between invocations.

  if (_apiPollTimer >= POLL_INTERVAL_SECONDS * 1000) {
    if (IsWiFiConnected()) {
      UpdateValueFromAPI();

      if (_currentValue[_selectedValueIndex] == "Unknown") {
        WriteToLCD("Invalid API", "response");
      }
      else if (_currentValueUpdated[_selectedValueIndex]) {
        WriteToLCD(_valueLabel[_selectedValueIndex], _currentValue[_selectedValueIndex], true);
        _currentValueUpdated[_selectedValueIndex] = false;
      }
    }
    else {
      WriteToLCD("WiFi conn lost");
      strncpy(_currentValue[_selectedValueIndex], "NULL", MAX_VALUE_LENGTH);
      InitializeWiFi();
    }

    _apiPollTimer = 0;
  }
}

/*
* Non-blocking check on the status of the buttons.
* Checks if either button is being pushed or released and calls associated function.
*/
void ProcessButtons() {
  static const int debounceDelay = 50;             // How long the button pin must read (in milliseconds) without value fluctuation to be considered "input".

  static elapsedMillis btn1LastChanged = 10000;   // Init all three to a "long ago" value. Statics.
  static elapsedMillis btn2LastChanged = 10000;
  
  static bool lastBtn1State = LOW;                // Statics, will retain value between method invocations.
  static bool lastBtn2State = LOW;
  
  // Button 1 processing
  bool currentBtn1State = (digitalRead(BTN_1_PIN));
  if (currentBtn1State != lastBtn1State) {
    btn1LastChanged = 0;
  }
  if (btn1LastChanged >= debounceDelay && currentBtn1State == HIGH) {
    Button1Pressed();
    btn1LastChanged = 0;
  }
  lastBtn1State = currentBtn1State;

  // Button 2 processing
  bool currentBtn2State = (digitalRead(BTN_2_PIN));
  if (currentBtn2State != lastBtn2State) {
    btn2LastChanged = 0;
  }
  if (btn2LastChanged >= debounceDelay && currentBtn2State == HIGH) {
    Button2Pressed();
    btn2LastChanged = 0;
  }
  lastBtn2State = currentBtn2State;
}

/*
* Called when button 1 is pressed for at least 50 ms.
* Currently configured to cycle through the various LDR based LCD dimming modes.
*/
void Button1Pressed() {
  if (_selectedDisplayMode == On) {
    WriteToLCD("LCD backlight", "always off");
    delay(2000);
    _selectedDisplayMode = Off;
    _lcd.noBacklight();
    _lcdBacklightOn = false;
  }

  else if (_selectedDisplayMode == Off) {
    WriteToLCD("LCD backlight", "Auto (light dep)");
    delay(2000);
    _selectedDisplayMode = Auto;
    if (analogRead(LDR_PIN < LDR_THRESHOLD)) {
      _lcd.noBacklight();
      _lcdBacklightOn = false;
    }
    else {
      _lcd.backlight();
      _lcdBacklightOn = true;
    }  
  }

  else if (_selectedDisplayMode == Auto) {
    WriteToLCD("LCD backlight", "always on");
    delay(2000);
    _selectedDisplayMode = On;
    _lcd.backlight();
    _lcdBacklightOn = true;
  }

  SaveConfigToEEPROM();
}

/*
* Called when button 2 is pressed for at least 50ms.
*/
void Button2Pressed() {
  // Currently this does nothing. Provision for future.
}

/*
* Non-blocking check on the status of the LDR.
* Used to check whether the user is swiping or has swiped across the sensor, as well as the current level of room brightness.
* If user swipe is detected, LDRSwiped() is called. Otherwise if the room is in a dark state, the backlight will be turned off if configured to do so.
*/
void ProcessLDR() {
  static const int debounceDelay = 50;                            // How long the LDR must read a consistant high or low value to be considered stable input.
  static const int minSwipeDarknessTime = 200;                    // The minimum amount of time (ms) the LDR must be in dark state to consider a thumb swipe as having stated.
  static const int maxSwipeDarknessTime = 3000;                   // After this time (ms) it is unlikely to be a thumb swipe and more likely to be the lights turning off.

  static elapsedMillis debounceTimer;                             // Timer for debouncing LDR readings.
  static bool previouslyReadingDarkness;                          // LDR reading from the previous invocation. Used for debouncing, not for tracking complete state changes.

  static elapsedMillis darkTimer;                                 // Timer for tracking darkness duration.
  static bool previouslyInDarkness = false;                       // Tracks if LDR has been in stable darkness. Not to be confused with previouslyReadingDarkness.

  bool readingDarkness = analogRead(LDR_PIN) < LDR_THRESHOLD;
  
  if (readingDarkness != previouslyReadingDarkness) {             // Debounce the LDR input.
    debounceTimer = 0;
  }

  if (debounceTimer > debounceDelay) {                            // Readings have stabilized.
    if (readingDarkness && !previouslyInDarkness) {               // Moving from light to dark.
      previouslyInDarkness = true;                                // We have moved into a dark state.
      darkTimer = 0;                                              // Begin tracking how long we have been in darkness.
    }

    if (readingDarkness && previouslyInDarkness) {                // We are currently in darkness, and have been for a while.
      if (darkTimer > maxSwipeDarknessTime && _lcdBacklightOn) {  // Have been in darkness long enough to turn off backlight.
        if (_selectedDisplayMode == Auto) {                       // Respect user choice.
          _lcd.noBacklight();
          _lcdBacklightOn = false;
        }
      }
    }

    if (!readingDarkness && previouslyInDarkness) {               // Moving from dark to light.
      previouslyInDarkness = false;                               // Reset for next loop.
      if (darkTimer >= minSwipeDarknessTime && darkTimer <= maxSwipeDarknessTime) {
        LDRSwiped();                                              // Between 200ms and 3000ms, likely a thumb swipe.
      }
      else if (darkTimer > maxSwipeDarknessTime) {                // Was in darkness for more than 3000ms, likely lights were off and now are back on.
        if (_selectedDisplayMode == Auto) {                       // Respect user choice.
          _lcd.backlight();
          _lcdBacklightOn = true;
        }
      }
    }
  }

  previouslyReadingDarkness = readingDarkness;                    // Update value for next loop debouncing.
}

/*
* Called when a users thumb swipe across the LDR is detected.
* Thumb swipe should put the LDR into a dark state for between 200 and 3000ms.
*/
void LDRSwiped() {
  _selectedValueIndex++;                          // Increment the value to be shown. Wrap around if we move out of range.
  if (_selectedValueIndex >= API_VALUE_COUNT) {
    _selectedValueIndex = 0;
    SaveConfigToEEPROM();
  }

  WriteToLCD(                                     // Display the summary for the currently selected option.
    _valueSelectionSummary[_selectedValueIndex][0],
    _valueSelectionSummary[_selectedValueIndex][1]
  );
}

/*
* Configures pinMode etc for the various button and LDR pins.
*/
void InitializeInputDevices() {
  pinMode(BTN_1_PIN, INPUT);
  pinMode(BTN_2_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
}

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
* Returns true if WiFi is in state WL_CONNECTED. False otherwise.
*/
bool IsWiFiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

/*
* Initializes the WiFi on the ESP. Attempts to connect using saved WiFi name and password.
* If connection fails, a fallback hotspot is spun up, including a configuration web portal.
* This method can also be used to attempt WiFi reconnections.
*/
void InitializeWiFi() {
  DEBUG_SERIAL.println("Initializing WiFi");
  WriteToLCD("WiFi connecting");
  WiFi.mode(WIFI_STA);

  // Attempt connection using stored WiFi configuration.
  // This allows us to resolve connection drops without invoking WiFiManager.
  WiFi.reconnect();
  elapsedMillis autoConnectMillis = 0;
  while (autoConnectMillis < WIFI_RECONN_TIMEOUT * 1000) {
    if (IsWiFiConnected()) {
      DEBUG_SERIAL.println("WiFi connected!");
      WriteToLCD("WiFi connected!");
      return;
    }
    delay(100);
  }

  // WiFi auto-connection wasn't successful. Spin up portal for config.
  DEBUG_SERIAL.println("WiFi connection failed");
  WriteToLCD("Automatic WiFi", "reconnect failed");
  delay(3000);

  DEBUG_SERIAL.println("Invoking WiFi configuration portal");
  WriteToLCD("Generating WiFi", "config portal");
  delay(3000);

  // Generate an instance of WiFi Manager to build the portal and handle reconnect.
  WiFiManager wifiManager;
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.autoConnect("ESP-CX-CTR", SECRET_WIFI_PASSWORD); // This will actually attempt reconnection one more time. It's possible to use .startConfigPortal() but the non-blocking code is a lot more complex.

  char passwordText[LCD_COLUMNS + 1];                          // Create a char[] to build password text. C does not support string concatenation for string literals directly.
  sprintf(passwordText, "Pass: %s", SECRET_WIFI_PASSWORD);     // Build the text to be displayed on the LCD and store in the char[].
  
  // Non-blocking WiFi Manager instance to allow us to refresh the display while the portal is active.
  autoConnectMillis = 0;
  while (WiFi.status() != WL_CONNECTED) {
    wifiManager.process();

    WriteToLCD("Connect to the", "following WiFi:");
    delay(2000);
    WriteToLCD("Name: ESP-CX-CTR", passwordText);
    delay(5000);

    WriteToLCD("Then visit the", "following site:");
    delay(2000);
    WriteToLCD("URL:", "192.168.4.1");
    delay(4000);

    // Periodically re-attempt connection to the saved WiFi... Just in case.
    if (autoConnectMillis > WIFI_RECONN_TIMEOUT * 1000) {
      WiFi.reconnect();
      autoConnectMillis = 0;
    }
  }

  DEBUG_SERIAL.println("WiFi connected!");
  WriteToLCD("WiFi connected!");
}

/*
* Polls the API for updated values to store in the _currentValues array.
* Response is validated, transformed and split based on the | operator.
* Booleans indicating which values have changed since the previous request are stored in _currentValueUpdated.
*/
void UpdateValueFromAPI() {
  _lcd.setCursor(15, 1); // Little dot in bottom right section shows API being polled.
  _lcd.print(".");

  static char responseBuffer[RESPONSE_BUFFFER_SIZE];                            // For manipulating the response from the API.
  static char previousValue[API_VALUE_COUNT][MAX_VALUE_LENGTH] = {"", "", ""};  // Holds the values that were previously in the _currentValues array for comparison.

  WiFiClientSecure client;
  HTTPClient https;

  client.setInsecure();
  https.begin(client, SECRET_API_ENDPOINT);
  https.addHeader("X-API-KEY", SECRET_API_KEY);

  DEBUG_SERIAL.println("Submitting request");
  int httpResponseCode = https.GET();
  DEBUG_SERIAL.print("Response code: ");
  DEBUG_SERIAL.println(httpResponseCode);

  if (httpResponseCode > 0) {
    ReadResponseStream(https, responseBuffer, RESPONSE_BUFFFER_SIZE);       // Reads the API response into the responseBuffer char array.
    RemoveAsteriskNotation(responseBuffer);                                 // Removes the * used to inform the 7-seg display which value to display. Unused on LCD units.
    bool validResponse = ValidatePayloadFormat(responseBuffer);             // Ensures there are at least as many values as we specify in API_VALUE_COUNT.

    char* token = strtok(responseBuffer, "|");                              // Gets the first token (the first value in the string prior to the first '|' operator).
    int index = 0;

    while (token != nullptr && index < API_VALUE_COUNT) {                   // While there are values to be read (ValidatePayloadFormat() should have handled this) and we haven't read all the values we expect...
      if (strcmp(previousValue[index], token) != 0) {                       // If the value differs from what it was previously...
        _currentValueUpdated[index] = true;                                 // Mark as updated.
        strncpy(_currentValue[index], token, MAX_VALUE_LENGTH);             // Copy the new value into the _currentValue array for eventual displaying.
        _currentValue[index][16] = '\0';                                    // Ensure null termination

        strncpy(previousValue[index], _currentValue[index], 16);            // Update the previous value to the current value.
        previousValue[index][16] = '\0';                                    // Ensure null termination.

        DEBUG_SERIAL.print("Got new value: ");
        DEBUG_SERIAL.print(token);
        DEBUG_SERIAL.print(" for index ");
        DEBUG_SERIAL.println(index);
      }
      else {
        _currentValueUpdated[index] = false;                                // No change detected for this stat.

        DEBUG_SERIAL.print("Polled API and received same value as previously (");
        DEBUG_SERIAL.print(token);
        DEBUG_SERIAL.print(") for index ");
        DEBUG_SERIAL.println(index);
      }

      token = strtok(nullptr, "|");                                         // Get the next token/value
      index++;
    }
  }
  else {
    DEBUG_SERIAL.println("Unable to contact API");                          // In the event we have WiFi but the API is unreachable
    for (int i = 0; i < API_VALUE_COUNT; i++) {
      strncpy(_currentValue[i], "Unknown", MAX_VALUE_LENGTH);
    }
  }

  https.end();

  _lcd.setCursor(15, 1);
  _lcd.print(" ");
}

/*
* Reads the response stream from the API into a character buffer and then terminates it.
*/
void ReadResponseStream(HTTPClient& https, char* buffer, int bufferSize) {
  WiFiClient* stream = https.getStreamPtr();                    // Gets the pointer to the stream of contents waiting to be read.
  
  int bytesRead = 0;
  while (stream->available() && bytesRead < bufferSize - 1) {   // While there is data available in the stream and space in the buffer array...
    buffer[bytesRead] = stream->read();                         // Read each byte into the buffer.
    bytesRead++;
  }

  buffer[bytesRead] = '\0';                                     // Ensure null termination of the string.
}

/*
* Iterates across the payload from the API and removes the first instance of the '*' character.
* This character is returned by the API for use in another project but isn't relevant here.
*/
void RemoveAsteriskNotation(char* buffer) {
  for (int i = 0; i < RESPONSE_BUFFFER_SIZE; i++) {           // For each index in the buffer array...
    if (buffer[i] == '\0') {                                  // If the end of the string is reached then exit.
      return;
    }
    if (buffer[i] == '*') {                                   // Once the '*' is found...
      for (int j = i; j < RESPONSE_BUFFFER_SIZE - 1; j++) {   // Starting at the current index, work along the string and shift all characters left, overwriting the '*'.
        buffer[j] = buffer[j + 1];
        if (buffer[j] == '\0') {                              // If the end of the string has been reached then return.
          return;
        }
      }
      buffer[RESPONSE_BUFFFER_SIZE - 1] = '\0';               // Otherwise, if the end of the array is reached (minus the removed '*') terminate the string and return.
      return;
    }
  }
}

/*
* Iterates across the payload from the API. Validates there are enough pipe delimiters for there to be at least as many values from the API as we are expecting.
*/
bool ValidatePayloadFormat(char* buffer) {
  int pipeCount = 0;
  for (int i= 0; i < RESPONSE_BUFFFER_SIZE; i++) {    // For each character in the buffer...
    if (buffer[i] == '\0') {                          // If we reached the end of the buffer without returning true, there weren't enough values in the payload.
      return false;
    }
    if (buffer[i] == '|') {                           // If we find a pipe delimiter, we have found a value preceeding it.
      pipeCount++;
    }
    if (pipeCount == API_VALUE_COUNT - 1) {           // There is one less pipe delimiter than there are values. Finding this many means we have enough values.
      return true;
    }
  }
  return false;
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

/*
* Pulls configuration from EEPROM on ESP startup and reads them into the global variables.
*/
void LoadConfigFromEEPROM() {
  _selectedValueIndex = EEPROM.readInt(0);
  _selectedDisplayMode = static_cast<DisplayDimmingMode>(EEPROM.readInt(1));
}

/*
* Saves configuration to EEPROM when config edited via bluetooth command.
*/
void SaveConfigToEEPROM() {
  EEPROM.writeInt(0, _selectedValueIndex);
  EEPROM.writeInt(1, _selectedDisplayMode);
}

/*
* Initializes the EEPROM, sets all addresses to 0 and then loads in default config.
*/
void InitializeEEPROM() {
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  };
  SaveConfigToEEPROM();
}
#pragma endregion FunctionDefinitions