#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =========================
// PIN CONFIG (Node B)
// =========================
#define IR_PIN 2

#define SDA_PIN 9
#define SCL_PIN 8

#define ENA_PIN 35 // PWM
#define IN1_PIN 36
#define IN2_PIN 37

LiquidCrystal_I2C lcd(0x27, 16, 2);

// PWM
const int PWM_CH = 0;
const int PWM_FREQ = 20000;
const int PWM_RES = 8; // 0..255
int pwmValue = 150;

// COUNTER
uint32_t counter = 0;
int lastIR = HIGH;
unsigned long lastLcdMs = 0;
const unsigned long LCD_INTERVAL = 200;

void motorForward(int pwm)
{
  pwm = constrain(pwm, 0, 255);
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  ledcWrite(PWM_CH, pwm);
}

void setup()
{
  Serial.begin(115200);
  delay(800);

  // IR
  pinMode(IR_PIN, INPUT_PULLUP);

  // Motor driver pins
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);

  ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENA_PIN, PWM_CH);

  motorForward(pwmValue);

  // LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  Serial.println("Node B Ready!");
  Serial.println("Ketik PWM 0..255 lalu Enter.");

  // Tampil awal
  lcd.setCursor(0, 0);
  lcd.print("Count: 0");
  lcd.setCursor(0, 1);
  lcd.print("PWM: ");
  lcd.print(pwmValue);
}

void loop()
{
  // ===== IR Count (simple edge detect) =====
  int irNow = digitalRead(IR_PIN);

  // Hitung saat HIGH -> LOW (barang lewat)
  if (lastIR == HIGH && irNow == LOW)
  {
    counter++;
    Serial.print("COUNT: ");
    Serial.println(counter);

    // anti double count (atur sesuai kecepatan conveyor)
    delay(80);
  }
  lastIR = irNow;

  // ===== Serial PWM input =====
  if (Serial.available())
  {
    int val = Serial.parseInt();
    if (val >= 0 && val <= 255)
    {
      pwmValue = val;
      motorForward(pwmValue);
      Serial.print("PWM updated: ");
      Serial.println(pwmValue);
    }
    else
    {
      Serial.println("Input PWM harus 0..255");
    }
    // bersihin sisa newline
    while (Serial.available())
      Serial.read();
  }

  // ===== LCD update (limit refresh) =====
  if (millis() - lastLcdMs >= LCD_INTERVAL)
  {
    lastLcdMs = millis();

    lcd.setCursor(0, 0);
    lcd.print("Count: ");
    lcd.print(counter);
    lcd.print("     "); // bersihin sisa digit

    lcd.setCursor(0, 1);
    lcd.print("PWM: ");
    lcd.print(pwmValue);
    lcd.print("     ");
  }
}