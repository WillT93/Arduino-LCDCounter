#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <elapsedMillis.h>

#include "globals_t93.h"
#include "lcd_t93.h"
#include "secrets_t93.h"
#include "wifi_t93.h"

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
  elapsedMillis timer = 0;
  while (timer < WIFI_RECONN_TIMEOUT * 1000) {
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
  wifiManager.setConfigPortalTimeout(60);
  wifiManager.setConfigPortalTimeoutCallback(PortalTimeoutCallback);
  wifiManager.setAPClientCheck(true);
  wifiManager.startConfigPortal("ESP-CX-CTR", SECRET_WIFI_PASSWORD);

  char passwordText[LCD_COLUMNS + 1];                           // Create a char[] to build password text. C does not support string concatenation for string literals directly.
  sprintf(passwordText, "Pass: %s", SECRET_WIFI_PASSWORD);      // Build the text to be displayed on the LCD and store in the char[].
  
  // Non-blocking WiFi Manager instance to allow us to refresh the display while the portal is active.
  timer = 0;
  int messageCount = 0;
  while (!IsWiFiConnected()) {
    wifiManager.process();

    // Non-blocking LCD printing. Allows WifiManager to process quickly, only updating the LCD every 2-6-2-6 seconds.
    if (timer > 16000) {
      timer = 0;
      messageCount = 0;
    }
    else if (timer > 10000 && messageCount == 3) {
      WriteToLCD("URL:", "192.168.4.1");
      messageCount++;
    }
    else if (timer > 8000 && messageCount == 2) {
      WriteToLCD("Then visit the", "following site:");
      messageCount++;
    }
    else if (timer > 2000 && messageCount == 1) {
      WriteToLCD("Name: ESP-CX-CTR", passwordText);
      messageCount++;
    }
    else if (messageCount == 0) {
      WriteToLCD("Connect to the", "following WiFi:");
      messageCount++;
    }
  }

  DEBUG_SERIAL.println("WiFi connected!");
  WriteToLCD("WiFi connected!");
  DEBUG_SERIAL.println("WiFi initialized");
}

/*
* Called when the ESP cannot connect to saved WiFi, the portal timed out and no clients were connected to the AP.
*/
void PortalTimeoutCallback() {
  DEBUG_SERIAL.println("WiFi config portal timeout - rebooting ESP");
  WriteToLCD("WiFi timeout", "rebooting...");
  ESP.restart();
}

/*
* Returns true if WiFi is in state WL_CONNECTED. False otherwise.
*/
bool IsWiFiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}
