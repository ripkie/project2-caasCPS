#include <Arduino.h>
#include <Wire.h>
#define BUTTON_PIN 4

int lastButtonState = LOW;

void setup()
{
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT);
  Serial.println("Node A Ready");
}

void loop()
{
  int buttonState = digitalRead(BUTTON_PIN);

  // trigger
  if (buttonState == HIGH && lastButtonState == LOW)
  {
    Serial.println("EMERGENCY BUTTON PRESSED");
  }

  lastButtonState = buttonState;
  delay(10);
}