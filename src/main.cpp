#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include "secrets.h"

// Debugging configuration
#define DEBUG                 false             // Optional printing of debug messages to serial
#define DEBUG_SERIAL          if (DEBUG) Serial

// LCD configuration
#define LCD_COLUMNS           16
#define LCD_ROWS              2
#define LCD_ADDRESS           0x27

// General configuration
#define POLL_INTERVAL_SECONDS 10

// Global vars
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
String currentValue;
bool currentValueUpdated;

// Function declarations
bool ValidateWiFiConnection();
void UpdateValueFromAPI();
void WriteToLCD(String, String = "");

void setup() {
  DEBUG_SERIAL.begin(9600);

  DEBUG_SERIAL.print("Initializing LCD");
  lcd.init();
  lcd.backlight();
  DEBUG_SERIAL.print("LCD initialized!");

  DEBUG_SERIAL.print("Initializing WiFi");
  DEBUG_SERIAL.print("Connecting");
  WriteToLCD("WiFi connecting", SECRET_WIFI_SSID);
  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG_SERIAL.print(".");
  }
  DEBUG_SERIAL.println();
  DEBUG_SERIAL.println("WiFi connected!");
  WriteToLCD("WiFi connected!");
}

void loop() {
  if (ValidateWiFiConnection()) {
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
    WiFi.reconnect();
  }

  delay(POLL_INTERVAL_SECONDS * 1000);
}

bool ValidateWiFiConnection() {
  return (WiFi.status() == WL_CONNECTED);
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
      DEBUG_SERIAL.print("Polled API and received same value as previously");
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

void WriteToLCD(String topRow, String bottomRow) {
  lcd.clear();

  for (int i = 0; i < LCD_COLUMNS; i++) {
    lcd.setCursor(i, 0);
    lcd.print(0);
    lcd.setCursor(i, 1);
    lcd.print(0);
    delay(50);
  }

  for (int i = LCD_COLUMNS - 1; i >= 0; i--) {
    lcd.setCursor(i, 0);
    lcd.print(" ");
    lcd.setCursor(i, 1);
    lcd.print(" ");
    delay(50);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(topRow);
  lcd.setCursor(0, 1);
  lcd.print(bottomRow);
}