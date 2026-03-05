#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// PIN CONFIG
#define IR_PIN 2
#define SDA_PIN 9
#define SCL_PIN 8

#define ENA_PIN 35
#define IN1_PIN 36
#define IN2_PIN 37

LiquidCrystal_I2C lcd(0x27, 16, 2);

// PWM
const int PWM_CH = 0;
const int PWM_FREQ = 20000;
const int PWM_RES = 8;
int pwmValue = 150;

// COUNTER
uint32_t counter = 0;
int lastIR = HIGH;

unsigned long lastLcdMs = 0;
const unsigned long LCD_INTERVAL = 200;

// ESP-NOW DATA
typedef struct
{
  uint8_t emergency;
} EspNowMsg;

bool emergencyStop = false;

// MOTOR CONTROL
void motorForward(int pwm)
{
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  ledcWrite(PWM_CH, pwm);
}

void motorStop()
{
  ledcWrite(PWM_CH, 0);
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
}

// LCD
void showEmergency()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("EMERGENCY STOP!!");
  lcd.setCursor(0, 1);
  lcd.print("Motor berhenti");
}

// ESP NOW RECEIVE
void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len)
{
  if (len != sizeof(EspNowMsg))
    return;

  EspNowMsg msg;
  memcpy(&msg, data, sizeof(msg));

  emergencyStop = msg.emergency;

  if (emergencyStop)
  {
    motorStop();
    showEmergency();
    Serial.println("EMERGENCY RECEIVED");
  }
  else
  {
    motorForward(pwmValue);
    lcd.clear();
    Serial.println("SYSTEM NORMAL");
  }
}

// SETUP
void setup()
{
  Serial.begin(115200);

  pinMode(IR_PIN, INPUT_PULLUP);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);

  ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENA_PIN, PWM_CH);

  // LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  // WIFI + ESPNOW
  WiFi.mode(WIFI_STA);
  Serial.print("NodeB MAC: ");
  Serial.println(WiFi.macAddress());
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP NOW FAILED");
    return;
  }

  esp_now_register_recv_cb(onEspNowRecv);

  Serial.println("NodeB Ready");

  motorForward(pwmValue);
}

// =========================
// LOOP
// =========================
void loop()
{
  if (emergencyStop)
    return;

  int irNow = digitalRead(IR_PIN);

  if (lastIR == HIGH && irNow == LOW)
  {
    counter++;
    Serial.print("COUNT: ");
    Serial.println(counter);
    delay(80);
  }

  lastIR = irNow;

  if (millis() - lastLcdMs > LCD_INTERVAL)
  {
    lastLcdMs = millis();

    lcd.setCursor(0, 0);
    lcd.print("Count: ");
    lcd.print(counter);
    lcd.print("   ");

    lcd.setCursor(0, 1);
    lcd.print("PWM: ");
    lcd.print(pwmValue);
    lcd.print("   ");
  }
}