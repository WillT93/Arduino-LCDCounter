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

// Debugging configuration
#define DEBUG                 true              // Optional printing of debug messages to serial
#define DEBUG_SERIAL          if (DEBUG) Serial

// LCD configuration
#define LCD_COLUMNS           16
#define LCD_ROWS              2
#define LCD_ADDRESS           0x27

// WiFi configuration
#define WIFI_CONN_TIMEOUT     10

// General configuration
#define POLL_INTERVAL_SECONDS 30

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
  DEBUG_SERIAL.println("LCD initialized!");
}

bool IsWiFiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

void InitializeWiFi() {
  DEBUG_SERIAL.println("Initializing WiFi");
  WriteToLCD("WiFi connecting");
  WiFi.mode(WIFI_STA);

  // Attempt connection using stored WiFi configuration
  WiFi.reconnect();
  elapsedMillis autoConnectMillis = 0;
  while (autoConnectMillis < WIFI_CONN_TIMEOUT * 1000) {
    if (WiFi.status() == WL_CONNECTED) {
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

  WiFiManager wifiManager;
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.autoConnect("ESP-CX-CTR", SECRET_WIFI_PASSWORD);

  String passwordString = "Pass: ";
  passwordString += SECRET_WIFI_PASSWORD;
  
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

    WiFi.reconnect();
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
  lcd.clear();

  if (animate) {
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
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(topRow);
  lcd.setCursor(0, 1);
  lcd.print(bottomRow);
}