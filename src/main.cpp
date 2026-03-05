#include <Arduino.h>
#include <Wire.h>

#define BUTTON_PIN 4

int lastReading = LOW;
int stableState = LOW;
unsigned long lastDebounceMs = 0;
const unsigned long debounceMs = 30;

void setup()
{
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT);
  Serial.println("Node A Ready (debounce)");
}

void loop()
{
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastReading)
  {
    lastDebounceMs = millis();
    lastReading = reading;
  }

  if (millis() - lastDebounceMs > debounceMs)
  {
    if (reading != stableState)
    {
      stableState = reading;

      // trigger hanya saat jadi HIGH (baru ditekan)
      if (stableState == HIGH)
      {
        Serial.println("EMERGENCY BUTTON PRESSED");
      }
    }
  }
}