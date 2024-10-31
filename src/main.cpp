#include <Arduino.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

#include <elapsedMillis.h>

#include "secrets.h"

#define LEN(arr) ((int) (sizeof (arr) / sizeof (arr)[0]))

// Debugging configuration
#define DEBUG                 true              // Optional printing of debug messages to serial.
#define DEBUG_SERIAL          if (DEBUG) Serial

// LCD configuration
#define ANIM_FRAME_COUNT      8
#define LCD_COLUMNS           16
#define LCD_ROWS              2
#define LCD_ADDRESS           0x27

// WiFi configuration
#define WIFI_RECONN_TIMEOUT   10                // How long to attempt WiFi connection with saved credentials before invoking portal. Also how often it will wait between re-attempts when portal is running.

// General configuration
#define POLL_INTERVAL_SECONDS 30                // How often to poll the endpoint.

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

// Global vars
hd44780_I2Cexp lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
String currentValue;
bool currentValueUpdated;

// Function declarations
bool IsWiFiConnected();
void InitializeLCD();
void InitializeWiFi();
void UpdateValueFromAPI();
void WriteToLCD(String, String = "", bool = false);
void PerformLCDAnimation();

void setup() {
  DEBUG_SERIAL.begin(9600);

  InitializeLCD();
  InitializeWiFi();
}

void loop() {
  if (IsWiFiConnected()) {
    UpdateValueFromAPI();

    if (currentValue == "Unknown") {
      WriteToLCD("Invalid API", "response");
    }
    else if (currentValueUpdated) {
      WriteToLCD("Subscriber count", currentValue);
      currentValueUpdated = false;
    }
  }
  else {
    WriteToLCD("WiFi conn lost");
    currentValue = "NULL";
    InitializeWiFi();
  }

  delay(POLL_INTERVAL_SECONDS * 1000);
}

void InitializeLCD() {
  DEBUG_SERIAL.println("Initializing LCD");
  
  lcd.init();
  lcd.backlight();
  
  // Import the custom chars into the LCD config.
  for (int i = 0; i < LEN(animationCustomChars); i++) {
    lcd.createChar(i, animationCustomChars[i]);
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
  lcd.setCursor(15, 1);
  lcd.print(".");

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
    String value = https.getString(); 
    if (value != currentValue) {
      currentValue = value;
      currentValueUpdated = true;

      DEBUG_SERIAL.print("Got new value: ");
      DEBUG_SERIAL.println(value);
    }
    else {
      DEBUG_SERIAL.println("Polled API and received same value as previously");
    }
  }
  else {
    DEBUG_SERIAL.println("Unable to contact API");
    currentValue = "Unknown";
  }

  https.end();

  lcd.setCursor(15, 1);
  lcd.print(" ");
}

void WriteToLCD(String topRow, String bottomRow, bool animate) {
  if (animate) {
    PerformLCDAnimation();
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(topRow);
  lcd.setCursor(0, 1);
  lcd.print(bottomRow);
}

void PerformLCDAnimation() {
  lcd.clear();
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
          lcd.setCursor(column, row);
          lcd.write(animationSequence[frameIndex][row]);
        }
    }
    delay(100);
    }
  }
  lcd.clear();
}