#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define IR_PIN 2

#define SDA_PIN 9
#define SCL_PIN 8

#define ENA_PIN 35 // PWM
#define IN1_PIN 36
#define IN2_PIN 37

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int PWM_CH = 0;
const int PWM_FREQ = 20000;
const int PWM_RES = 8;
volatile int pwmValue = 150; // kecepatan motor

uint32_t counter = 0;
int lastIR = HIGH;

// anti double count (sesuaikan speed conveyor)
const unsigned long COUNT_LOCKOUT_MS = 80;
unsigned long lastCountMs = 0;

// LCD refresh normal
const unsigned long LCD_INTERVAL = 200;
unsigned long lastLcdMs = 0;

typedef struct __attribute__((packed))
{
  uint8_t emergency;
} EspNowMsg;

// state
volatile bool emergencyStop = false;
bool lastEmergencyStop = false;

// kontrol motor
void motorForward(int pwm)
{
  pwm = constrain(pwm, 0, 255);
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

// =========================
// LCD SCREENS
// =========================
void lcdShowEmergency()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("EMERGENCY STOP!!");
  lcd.setCursor(0, 1);
  lcd.print("Motor berhenti");
}

void lcdShowRunTemplate()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Count: ");
  lcd.setCursor(0, 1);
  lcd.print("PWM: ");
}

// update angka tanpa clear terus
void lcdUpdateRunValues()
{
  lcd.setCursor(7, 0);
  lcd.print(counter);
  lcd.print("     ");

  lcd.setCursor(5, 1);
  lcd.print((int)pwmValue);
  lcd.print("     ");
}

// ESP-NOW RECEIVE CALLBACK
// NOTE: jangan utak-atik LCD di sini (biar gak corrupt)
void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len)
{
  if (len != sizeof(EspNowMsg))
    return;

  EspNowMsg msg;
  memcpy(&msg, data, sizeof(msg));

  emergencyStop = (msg.emergency == 1);

  if (emergencyStop)
  {
    motorStop();
    Serial.println("ESP-NOW: EMERGENCY=1 (STOP)");
  }
  else
  {
    motorForward(pwmValue);
    Serial.println("ESP-NOW: EMERGENCY=0 (RUN)");
  }
}

// SETUP
void setup()
{
  Serial.begin(115200);
  delay(800);

  // IR
  pinMode(IR_PIN, INPUT_PULLUP);

  // Motor pins
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);

  // PWM init
  ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENA_PIN, PWM_CH);

  // LCD init
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  // WiFi + ESP-NOW
  WiFi.mode(WIFI_STA);
  Serial.print("NodeB MAC: ");
  Serial.println(WiFi.macAddress());

  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW init FAILED!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ESP-NOW FAIL");
    while (true)
      delay(100);
  }

  esp_now_register_recv_cb(onEspNowRecv);
  Serial.println("NodeB Ready (ESP-NOW receiver)");

  // Start in RUN mode
  motorForward(pwmValue);
  lcdShowRunTemplate();
  lcdUpdateRunValues();

  Serial.println("Ready: IR Count + PWM + LCD + ESP-NOW");
}

// LOOP
void loop()
{
  if (emergencyStop != lastEmergencyStop)
  {
    lastEmergencyStop = emergencyStop;

    if (emergencyStop)
    {
      lcdShowEmergency();
    }
    else
    {
      lcdShowRunTemplate();
      lcdUpdateRunValues();
    }
  }

  if (emergencyStop)
  {
    delay(20);
    return;
  }

  // ---- IR counting (edge detect + lockout) ----
  int irNow = digitalRead(IR_PIN);

  if (lastIR == HIGH && irNow == LOW)
  {
    unsigned long now = millis();
    if (now - lastCountMs >= COUNT_LOCKOUT_MS)
    {
      lastCountMs = now;
      counter++;
      Serial.print("COUNT: ");
      Serial.println(counter);
    }
  }
  lastIR = irNow;

  // ---- LCD update (periodic, no clear) ----
  if (millis() - lastLcdMs >= LCD_INTERVAL)
  {
    lastLcdMs = millis();
    lcdUpdateRunValues();
  }
}