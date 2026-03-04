#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =========================
// KONFIGURASI PIN
// =========================
#define IR_PIN 2
#define RELAY_PIN 26
#define SDA_PIN 9
#define SCL_PIN 8

#define TARGET_COUNT 12
#define DEBOUNCE_DELAY 50

LiquidCrystal_I2C lcd(0x27, 16, 2);

int counter = 0;
bool lastState = HIGH;
unsigned long lastDebounceTime = 0;

void setup()
{
  Serial.begin(115200);

  // Inisialisasi I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("   SYSTEM START   ");
  lcd.setCursor(0, 1);
  lcd.print("   Please Wait... ");
  delay(1500);

  // Setup Pin
  pinMode(IR_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);

  // Motor ON
  digitalWrite(RELAY_PIN, HIGH);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Barang: 0/");
  lcd.print(TARGET_COUNT);

  Serial.println("Sistem Ready!");
}

void loop()
{
  int sensor = digitalRead(IR_PIN);

  if (sensor == LOW && lastState == HIGH && (millis() - lastDebounceTime) > DEBOUNCE_DELAY)
  {
    lastDebounceTime = millis();
    counter++;

    Serial.print("Barang ke-");
    Serial.println(counter);

    lcd.setCursor(8, 0);
    lcd.print(counter);

    // TARGET TERCAPAI
    if (counter >= TARGET_COUNT)
    {
      Serial.println("=== TARGET TERCAPAI ===");

      digitalWrite(RELAY_PIN, LOW); // Motor OFF

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("TARGET ");
      lcd.print(TARGET_COUNT);
      lcd.print(" OK!");
      lcd.setCursor(0, 1);
      lcd.print("MOTOR BERHENTI");

      delay(3000);

      // Reset Counter
      counter = 0;
      digitalWrite(RELAY_PIN, HIGH); // Motor ON lagi

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Barang: 0/");
      lcd.print(TARGET_COUNT);

      Serial.println("Sistem Reset!");
    }
  }

  lastState = sensor;
}