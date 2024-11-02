#include <elapsedMillis.h>

#include "globals_t93.h"
#include "secrets_t93.h"
#include "wifi_t93.h"
#include "lcd_t93.h"
#include "api_t93.h"

/*
* Non-blocking check on whether the API needs polling.
* When the polling interval has been reached, the API will be contacted for a value update.
*/
void ProcessAPIPolling() {
  static elapsedMillis _apiPollTimer = 10000; // Initialize to a value ready for polling. Static so will retain any updated values between invocations.

  if (_apiPollTimer >= POLL_INTERVAL_SECONDS * 1000) {
    DEBUG_SERIAL.println("Beginning API polling process");
    if (IsWiFiConnected()) {
      DEBUG_SERIAL.println("WiFi validated");
      UpdateValueFromAPI();
    }
    else {
      DEBUG_SERIAL.println("WiFi connection failure");
      WriteToLCD("WiFi conn lost");
      strncpy(_currentValue[_selectedValueIndex], "NULL", MAX_VALUE_LENGTH);
      InitializeWiFi();
    }

    _apiPollTimer = 0;
  }
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

  client.setInsecure();                                                     // The endpoint being hit is an https endpoint, but it doesn't require certs etc.
  https.begin(client, SECRET_API_ENDPOINT);
  https.addHeader("X-API-KEY", SECRET_API_KEY);                             // Using X-API-KEY header as auth function on endpoint.

  DEBUG_SERIAL.println("Submitting request");
  int httpResponseCode = https.GET();
  DEBUG_SERIAL.print("Response code: ");
  DEBUG_SERIAL.println(httpResponseCode);

  if (httpResponseCode > 0) {
    ReadResponseStream(https, responseBuffer, RESPONSE_BUFFFER_SIZE);       // Reads the API response into the responseBuffer char array.
    RemoveAsteriskNotation(responseBuffer);                                 // Removes the * used to inform the 7-seg display which value to display. Unused on LCD units.
    bool validResponse = ValidatePayloadFormat(responseBuffer);             // Ensures there are at least as many values as API_VALUE_COUNT.

    if (!validResponse) {
      DEBUG_SERIAL.println("Invalid API response");
      for (int i = 0; i < API_VALUE_COUNT; i++) {
        strncpy(_currentValue[i], "Unknown", MAX_VALUE_LENGTH);
      }
    }

    char* token = strtok(responseBuffer, "|");                              // Gets the first token (the first value in the string prior to the first '|' operator).
    int index = 0;

    while (token != nullptr && index < API_VALUE_COUNT) {                   // While there are values to be read (ValidatePayloadFormat() should have handled this) and haven't read all the values expected...
      if (strcmp(previousValue[index], token) != 0) {                       // If the value differs from what it was previously...
        _currentValueUpdated[index] = true;                                 // Mark as updated.
        strncpy(_currentValue[index], token, MAX_VALUE_LENGTH);             // Copy the new value into the _currentValue array for eventual displaying.
        _currentValue[index][MAX_VALUE_LENGTH - 1] = '\0';                  // Ensure null termination

        strncpy(previousValue[index], _currentValue[index], 16);            // Update the previous value to the current value.
        previousValue[index][MAX_VALUE_LENGTH - 1] = '\0';                  // Ensure null termination.

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
    DEBUG_SERIAL.println("Unable to contact API");                          // In the event WiFi is connected but the API is unreachable
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
  DEBUG_SERIAL.println("Reading response stream from API");
  WiFiClient* stream = https.getStreamPtr();                    // Gets the pointer to the stream of contents waiting to be read.
  
  int bytesRead = 0;
  while (stream->available()) {                                 // While there is data available in the stream...
    if (bytesRead == bufferSize - 1) {                          // If end of buffer is reached...
      DEBUG_SERIAL.println("resonseBuffer exhausted prior to payload completion. Possibly this is fine id only using a subset of the first few values in the response.");
      break;
    }
    buffer[bytesRead] = stream->read();                         // Read each byte into the buffer.
    bytesRead++;
  }
  DEBUG_SERIAL.print("Read ");
  DEBUG_SERIAL.print(bytesRead);
  DEBUG_SERIAL.println(" bytes into responseBuffer");
  
  buffer[bytesRead] = '\0';                                     // Ensure null termination of the string.
}

/*
* Iterates across the payload from the API and removes the first instance of the '*' character.
* This character is returned by the API for use in another project but isn't relevant here.
*/
void RemoveAsteriskNotation(char* buffer) {
  DEBUG_SERIAL.println("Stripping asterisk notation");
  for (int i = 0; i < RESPONSE_BUFFFER_SIZE; i++) {           // For each index in the buffer array...
    if (buffer[i] == '\0') {                                  // If the end of the string is reached then exit.
      DEBUG_SERIAL.println("End of string located prior to asterisk");
      return;
    }
    if (buffer[i] == '*') {                                   // Once the '*' is found...
      DEBUG_SERIAL.print("Asterisk located at buffer index ");
      DEBUG_SERIAL.println(i);
      for (int j = i; j < RESPONSE_BUFFFER_SIZE - 1; j++) {   // Starting at the current index, work along the string and shift all characters left, overwriting the '*'.
        buffer[j] = buffer[j + 1];
        if (buffer[j] == '\0') {                              // If the end of the string has been reached then return.
          DEBUG_SERIAL.println("End of string located within buffer");
          return;
        }
      }
      buffer[RESPONSE_BUFFFER_SIZE - 1] = '\0';               // Otherwise, if the end of the array is reached (minus the removed '*') terminate the string and return.
      DEBUG_SERIAL.println("End of buffer reached before string termination character. This should never happen");
      return;
    }
  }
}

/*
* Iterates across the payload from the API. Validates there are enough pipe delimiters for there to be at least as many values from the API as expected in API_VALUE_COUNT.
*/
bool ValidatePayloadFormat(char* buffer) {
  DEBUG_SERIAL.println("Validating resulting payload format for sufficient API values");
  int pipeCount = 0;
  for (int i= 0; i < RESPONSE_BUFFFER_SIZE; i++) {    // For each character in the buffer...
    if (buffer[i] == '\0') {                          // If the end of the buffer is reached without returning true, there weren't enough values in the payload.
      DEBUG_SERIAL.println("Insufficient values located in responseBuffer");
      return false;
    }
    if (buffer[i] == '|') {                           // If pipe delimiter found, there is a value preceeding it.
      pipeCount++;
    }
    if (pipeCount == API_VALUE_COUNT - 1) {           // There is one less pipe delimiter than there are values. Finding this many means enough values have been located.
      DEBUG_SERIAL.println("Payload passed validation");
      return true;
    }
  }
  DEBUG_SERIAL.println("End of response buffer reached without sufficient values being located");
  return false;
}
