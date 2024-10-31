// TODO: migrate away from excessive usages of String to avoid memory fragmentation.
// TODO: LDR display blanking.
// TODO: LDR value selection.
// TODO: Configuration cycling for different LDR modes.
// TODO: Run packet analysis
// TODO: function docs
// TODO: char* vs char[]

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

#include "secrets.h"

#pragma region PreProcessorDirectives

// Helper methods
#define LEN(arr) ((int) (sizeof (arr) / sizeof (arr)[0]))

// Debugging configuration
#define DEBUG                 true              // Optional printing of debug messages to serial.
#define DEBUG_SERIAL          if (DEBUG) Serial
#define EEPROM_INIT           false             // When set to true, EEPROM is written over with 0's. Perform once per ESP32 unit.

// LCD configuration
#define ANIM_FRAME_COUNT      8                 // The number of frames in the LCD animation sequence.
#define LCD_COLUMNS           16
#define LCD_ROWS              2
#define LCD_ADDRESS           0x27

// WiFi configuration
#define WIFI_RECONN_TIMEOUT   10                // How long to attempt WiFi connection with saved credentials before invoking portal. Also how often it will wait between re-attempts when portal is running.

// General configuration
#define POLL_INTERVAL_SECONDS 30                // How often to poll the endpoint.
#define API_VALUE_COUNT       3                 // The number of values this version of code expects from the API, values in excess will be discarded. There should be a _valueLabel entry for each of these in secrets file.

#pragma endregion PreProcessorDirectives

#pragma region Constants

// The custom chars that make up the various animation frames.
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

// The characters that a single column must progress through in completion for one cycle of the animation. First digit is charater for upper row, second digit is for lower row.
const int animationSequence[ANIM_FRAME_COUNT][LCD_ROWS] =
{
  { 5, 3 },
  { 5, 4 },
  { 3, 0 },
  { 4, 0 },
  { 1, 0 },
  { 2, 0 },
  { 5, 1 },
  { 5, 2 },
};

#pragma endregion Constants

#pragma region Globals

hd44780_I2Cexp _lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
String _currentValue[API_VALUE_COUNT];                    // The current values available to be rendered on the display. One for each API value.
bool _currentValueUpdated[API_VALUE_COUNT];               // Whether the latest value received from the API differs from what is currently being rendered. One for each API value.
int _selectedValueIndex;                                  // The statistic chosen to be displayed. API returns multiple, pipe delimited ints. The one selected here is what is rendered on the display.

#pragma endregion Globals

#pragma region FunctionDeclarations

bool IsWiFiConnected();
void InitializeLCD();
void InitializeWiFi();
void UpdateValueFromAPI();
void WriteToLCD(String, String = "", bool = false);
void PerformLCDAnimation();
void InitializeEEPROM();
void SaveConfigToEEPROM();
void LoadConfigFromEEPROM();

#pragma endregion FunctionDeclarations

void setup() {
  if (EEPROM_INIT) {
    InitializeEEPROM();
    return;
  }

  DEBUG_SERIAL.begin(9600);

  InitializeLCD();
  InitializeWiFi();
}

void loop() {
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
    _currentValue[_selectedValueIndex] = "NULL";
    InitializeWiFi();
  }

  delay(POLL_INTERVAL_SECONDS * 1000);
}

#pragma region FunctionDefinitions

void InitializeLCD() {
  DEBUG_SERIAL.println("Initializing LCD");
  
  _lcd.init();
  _lcd.backlight();
  
  // Import the custom chars into the LCD config.
  for (int i = 0; i < LEN(animationCustomChars); i++) {
    _lcd.createChar(i, animationCustomChars[i]);
  }
  
  DEBUG_SERIAL.println("LCD initialized!");
}

bool IsWiFiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

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

  String passwordString = "Pass: ";
  passwordString += SECRET_WIFI_PASSWORD;
  
  // Non-blocking WiFi Manager instance to allow us to refresh the display while the portal is active.
  autoConnectMillis = 0;
  while (WiFi.status() != WL_CONNECTED) {
    wifiManager.process();

    WriteToLCD("Connect to the", "following WiFi:");
    delay(2000);
    WriteToLCD("Name: ESP-CX-CTR", passwordString);
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

void UpdateValueFromAPI() {
  _lcd.setCursor(15, 1); // Little dot in bottom right section shows API being polled.
  _lcd.print(".");

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
    String values = https.getString();
    values.remove(values.indexOf('*'), 1); // Removes the * used to inform the 7-seg display which value to display. Unused on LCD units.

    for (int i = 0; i < API_VALUE_COUNT; i++) {
      String newVal;
      if (i < API_VALUE_COUNT - 1) { // Copy the value up unto the next pipe into the array, then delete up to and including the pipe.
        int pipeIndex = values.indexOf('|');
        newVal = values.substring(0, pipeIndex);
        values.remove(0, pipeIndex + 1);
      } else { // Last value is treated differently as it has no closing pipe.
        values.trim();
        newVal = values;
      }

      if (newVal != _currentValue[i]) {
        _currentValue[i] = newVal;
        _currentValueUpdated[i] = true;

        DEBUG_SERIAL.print("Got new value: ");
        DEBUG_SERIAL.print(newVal);
        DEBUG_SERIAL.print(" for index ");
        DEBUG_SERIAL.println(i);
      }
      else {
        DEBUG_SERIAL.print("Polled API and received same value as previously (");
        DEBUG_SERIAL.print(newVal);
        DEBUG_SERIAL.print(") for index ");
        DEBUG_SERIAL.println(i);
      }
    }
  }
  else {
    DEBUG_SERIAL.println("Unable to contact API");
    for (int i = 0; i < API_VALUE_COUNT; i++) {
      _currentValue[i] = "Unknown";
    }
  }

  https.end();

  _lcd.setCursor(15, 1);
  _lcd.print(" ");
}

void WriteToLCD(String topRow, String bottomRow, bool animate) {
  if (animate) {
    PerformLCDAnimation();
  }

  _lcd.clear();
  _lcd.setCursor(0, 0);
  _lcd.print(topRow);
  _lcd.setCursor(0, 1);
  _lcd.print(bottomRow);
}

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
}

/*
* Saves configuration to EEPROM when config edited via bluetooth command.
*/
void SaveConfigToEEPROM() {
  EEPROM.writeInt(0, _selectedValueIndex);
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