#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>

#define IR_PIN 2
#define SDA_PIN 9
#define SCL_PIN 8
#define ENA_PIN 35
#define IN1_PIN 36
#define IN2_PIN 37

// WiFi config
#define WIFI_SSID "iPhone"
#define WIFI_PASS "12341234"
#define RAILWAY_URL "https://backendrailway-production-6b59.up.railway.app/api/data"
#define RAILWAY_CONTROL_URL "https://backendrailway-production-6b59.up.railway.app/api/control"

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int PWM_CH = 0;
const int PWM_FREQ = 20000;
const int PWM_RES = 8;

// =========================
// STATE
// =========================
volatile int pwmValue = 150; // kecepatan pwm

// Emergency hanya dari NodeA
volatile bool espNowEmergency = false;
volatile bool emergencyStop = false;

// web tidak lagi jadi sumber emergency
volatile bool cancelRequestFlag = false;

// IR interrupt
volatile bool irTriggered = false;
volatile unsigned long lastIrIsrMs = 0;

uint32_t counter = 0;
bool wifiConnected = false;
bool lastEmergencyStop = false;

// cache LCD supaya tidak update berlebihan
uint32_t lastShownCounter = 0xFFFFFFFF;
int lastShownPwm = -1;

// =========================
// TIMING
// =========================
const unsigned long COUNT_LOCKOUT_MS = 50;

const unsigned long SEND_INTERVAL = 1500; // POST tiap 1.5 detik
unsigned long lastSendMs = 0;

const unsigned long CONTROL_CHECK_INTERVAL = 1000; // GET control tiap 1 detik
unsigned long lastControlCheck = 0;

const unsigned long LCD_INTERVAL = 300;
unsigned long lastLcdMs = 0;

// =========================
// ESP-NOW STRUCT
// =========================
typedef struct __attribute__((packed))
{
  uint8_t emergency;
} EspNowMsg;

// function prototype
void updateEmergencyState();
void applyControl();
void sendToRailway();

// =========================
// INTERRUPT IR
// =========================
void IRAM_ATTR onIrDetected()
{
  unsigned long now = millis();
  if (now - lastIrIsrMs >= COUNT_LOCKOUT_MS)
  {
    lastIrIsrMs = now;
    irTriggered = true;
  }
}

// =========================
// MOTOR CONTROL
// =========================
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
// LCD FUNCTIONS
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

  lastShownCounter = 0xFFFFFFFF;
  lastShownPwm = -1;
}

void lcdUpdateRunValues(bool force = false)
{
  if (force || counter != lastShownCounter)
  {
    lcd.setCursor(7, 0);
    lcd.print("         ");
    lcd.setCursor(7, 0);
    lcd.print(counter);
    lastShownCounter = counter;
  }

  if (force || pwmValue != lastShownPwm)
  {
    lcd.setCursor(5, 1);
    lcd.print("     ");
    lcd.setCursor(5, 1);
    lcd.print((int)pwmValue);
    lastShownPwm = pwmValue;
  }
}

void lcdShowMacAddress()
{
  String mac = WiFi.macAddress();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MAC NodeB:");
  lcd.setCursor(0, 1);
  lcd.print(mac);
}

void lcdShowWifiStatus(bool connected)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(connected ? "WiFi OK!" : "WiFi FAILED!");

  lcd.setCursor(0, 1);
  if (connected)
    lcd.print(WiFi.localIP().toString());
  else
    lcd.print("Check SSID/Pass");
}

// =========================
// HELPER PARSE
// =========================
bool parseCancelFromResponse(const String &response)
{
  if (response.indexOf("\"cancel\":1") >= 0)
    return true;
  if (response.indexOf("\"cancel\": 1") >= 0)
    return true;
  if (response.indexOf("\"cancel\":true") >= 0)
    return true;
  if (response.indexOf("\"cancel\": true") >= 0)
    return true;
  return false;
}

int parsePwmFromResponse(const String &response, int defaultValue)
{
  int pwmKey = response.indexOf("\"pwm\":");
  if (pwmKey < 0)
    pwmKey = response.indexOf("\"pwm\": ");
  if (pwmKey < 0)
    return defaultValue;

  int start = response.indexOf(":", pwmKey);
  if (start < 0)
    return defaultValue;
  start++;

  while (start < (int)response.length() && response[start] == ' ')
    start++;

  int end = start;
  while (end < (int)response.length() && isDigit(response[end]))
    end++;

  String pwmStr = response.substring(start, end);
  if (pwmStr.length() == 0)
    return defaultValue;

  int newPwm = pwmStr.toInt();
  if (newPwm < 0 || newPwm > 255)
    return defaultValue;

  return newPwm;
}

// =========================
// ESP-NOW CALLBACK
// =========================
// Pakai signature lama supaya kompatibel
void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len)
{
  if (len != sizeof(EspNowMsg))
    return;

  EspNowMsg msg;
  memcpy(&msg, data, sizeof(msg));

  espNowEmergency = (msg.emergency == 1);

  Serial.printf("ESP-NOW RX -> emergency=%d from %02X:%02X:%02X:%02X:%02X:%02X\n",
                msg.emergency,
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// =========================
// RAILWAY
// =========================
void sendToRailway()
{
  if (!wifiConnected)
    return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, RAILWAY_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);

  String jsonData = "{";
  jsonData += "\"emergency\":" + String(emergencyStop ? 1 : 0) + ",";
  jsonData += "\"counter\":" + String(counter) + ",";
  jsonData += "\"pwm\":" + String(pwmValue);
  jsonData += "}";

  int httpCode = http.POST(jsonData);
  Serial.printf("Railway POST -> %d\n", httpCode);

  if (httpCode > 0)
  {
    String response = http.getString();
    Serial.printf("POST response: %s\n", response.substring(0, 80).c_str());
  }
  else
  {
    Serial.printf("Railway POST fail: %d -> %s\n",
                  httpCode,
                  http.errorToString(httpCode).c_str());
  }

  http.end();
}

void checkWebControl()
{
  if (!wifiConnected)
    return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, RAILWAY_CONTROL_URL);
  http.setTimeout(3000);

  int httpCode = http.GET();
  String response = "";

  if (httpCode > 0)
    response = http.getString();

  http.end();

  Serial.printf("Control GET -> %d | %s\n", httpCode, response.substring(0, 80).c_str());

  if (httpCode == 200)
  {
    bool changed = false;

    bool cancelRequest = parseCancelFromResponse(response);

    if (cancelRequest && espNowEmergency)
    {
      espNowEmergency = false;
      emergencyStop = false; // update langsung
      Serial.println("Emergency canceled from web");
      changed = true;
    }

    int newPwm = parsePwmFromResponse(response, pwmValue);
    if (newPwm >= 0 && newPwm <= 255)
    {
      if (abs(newPwm - pwmValue) > 2)
      {
        pwmValue = newPwm;
        Serial.printf("WEB PWM -> %d\n", pwmValue);
        changed = true;
      }
    }

    // kalau ada perubahan, kirim update ke dashboard secepatnya
    if (changed)
    {
      updateEmergencyState();
      applyControl();
      sendToRailway();
      lastSendMs = millis();
    }
  }
}
// =========================
// SYSTEM CONTROL
// =========================
void updateEmergencyState()
{
  emergencyStop = espNowEmergency;
}

void applyControl()
{
  updateEmergencyState();

  if (emergencyStop)
  {
    motorStop();
    return;
  }

  motorForward(pwmValue);
}

// =========================
// WIFI RECONNECT
// =========================
void ensureWiFi()
{
  static unsigned long lastReconnectAttempt = 0;
  static bool lastWiFiState = false;

  wifiConnected = (WiFi.status() == WL_CONNECTED);

  if (wifiConnected && !lastWiFiState)
  {
    Serial.println("WiFi connected/reconnected");
    Serial.print("WiFi channel: ");
    Serial.println(WiFi.channel());

    esp_wifi_set_channel(WiFi.channel(), WIFI_SECOND_CHAN_NONE);
    Serial.printf("ESP-NOW set to channel %d\n", WiFi.channel());
  }

  lastWiFiState = wifiConnected;

  if (wifiConnected)
    return;

  if (millis() - lastReconnectAttempt < 3000)
    return;
  lastReconnectAttempt = millis();

  Serial.println("WiFi disconnected, reconnecting...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

// =========================
// SETUP
// =========================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== NODEB STARTUP ===");

  pinMode(IR_PIN, INPUT_PULLUP);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);

  // PWM setup
  ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENA_PIN, PWM_CH);

  // LCD setup
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  // WiFi STA
  WiFi.mode(WIFI_STA);
  delay(100);

  Serial.print("NodeB MAC: ");
  Serial.println(WiFi.macAddress());

  lcdShowMacAddress();
  delay(1500);

  // Connect WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("Connecting WiFi '%s'", WIFI_SSID);

  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30)
  {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    wifiConnected = true;
    Serial.println("WiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("WiFi channel: ");
    Serial.println(WiFi.channel());

    lcdShowWifiStatus(true);
  }
  else
  {
    wifiConnected = false;
    Serial.println("WiFi FAILED!");
    lcdShowWifiStatus(false);
  }

  delay(1500);

  // Penting: samakan channel ESP-NOW dengan channel WiFi
  if (wifiConnected)
  {
    esp_wifi_set_channel(WiFi.channel(), WIFI_SECOND_CHAN_NONE);
    Serial.printf("ESP-NOW set to channel %d\n", WiFi.channel());
  }

  // ESP-NOW init
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
  Serial.println("ESP-NOW Ready");

  attachInterrupt(digitalPinToInterrupt(IR_PIN), onIrDetected, FALLING);

  motorForward(pwmValue);
  delay(500);

  lcdShowRunTemplate();
  lcdUpdateRunValues(true);

  Serial.println("NodeB READY");
  Serial.println("Copy MAC and WiFi channel to NodeA");
}

// =========================
// LOOP
// =========================
void loop()
{
  ensureWiFi();
  wifiConnected = (WiFi.status() == WL_CONNECTED);

  // proses hasil interrupt IR
  if (irTriggered)
  {
    noInterrupts();
    irTriggered = false;
    interrupts();

    counter++;
    Serial.printf("COUNT #%lu\n", counter);
  }

  // BACA CONTROL WEB LEBIH DULU
  if (millis() - lastControlCheck >= CONTROL_CHECK_INTERVAL)
  {
    lastControlCheck = millis();
    checkWebControl();
  }

  // Baru apply state terbaru
  applyControl();

  // Update LCD saat emergency berubah
  if (emergencyStop != lastEmergencyStop)
  {
    lastEmergencyStop = emergencyStop;

    if (emergencyStop)
      lcdShowEmergency();
    else
    {
      lcdShowRunTemplate();
      lcdUpdateRunValues(true);
    }
  }

  // Send Railway periodik
  if (millis() - lastSendMs >= SEND_INTERVAL)
  {
    lastSendMs = millis();
    sendToRailway();
  }

  // LCD refresh
  if (!emergencyStop && millis() - lastLcdMs >= LCD_INTERVAL)
  {
    lastLcdMs = millis();
    lcdUpdateRunValues();
  }

  delay(10);
}