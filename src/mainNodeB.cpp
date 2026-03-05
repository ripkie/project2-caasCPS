#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoWebsockets.h>

using namespace websockets;

// =========================
// WIFI + RAILWAY WS
// =========================
const char *ssid = "iPhone";
const char *password = "12341234";

// contoh: "wss://nama-project.up.railway.app"
const char *WS_URL = "wss://esp32-websocket.up.railway.app";

// client websocket
WebsocketsClient wsClient;
bool wsConnected = false;
unsigned long lastWsReconnectMs = 0;
const unsigned long WS_RECONNECT_INTERVAL = 3000;

// kirim status berkala ke server
unsigned long lastStatusSendMs = 0;
const unsigned long STATUS_INTERVAL = 400;

// =========================
// PIN CONFIG
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
const int PWM_RES = 8;
volatile int pwmValue = 150; // kecepatan motorrrrrr

// COUNTER
uint32_t counter = 0;
int lastIR = HIGH;

const unsigned long COUNT_LOCKOUT_MS = 80;
unsigned long lastCountMs = 0;

// LCD refresh
const unsigned long LCD_INTERVAL = 200;
unsigned long lastLcdMs = 0;

// ESP-NOW payload
typedef struct __attribute__((packed))
{
  uint8_t emergency;
} EspNowMsg;

// state
volatile bool emergencyStop = false;
bool lastEmergencyStop = false;

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
// LCD
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

void lcdUpdateRunValues()
{
  lcd.setCursor(7, 0);
  lcd.print(counter);
  lcd.print("     ");

  lcd.setCursor(5, 1);
  lcd.print((int)pwmValue);
  lcd.print("     ");
}

// =========================
// ESP-NOW CALLBACK
// jangan sentuh LCD di callback
// =========================
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

// =========================
// PARSE MESSAGE dari Railway
// format yang didukung:
// 1) "PWM:150"
// 2) JSON sederhana: {"type":"pwm","value":150}
// =========================
int parsePwmFromMessage(const String &msg)
{
  if (msg.startsWith("PWM:"))
  {
    return msg.substring(4).toInt();
  }

  // parse JSON simpel tanpa ArduinoJson
  // cari "value":
  int idx = msg.indexOf("\"value\"");
  if (idx >= 0)
  {
    int colon = msg.indexOf(':', idx);
    if (colon >= 0)
    {
      String num = msg.substring(colon + 1);
      num.replace("}", "");
      num.replace(" ", "");
      num.replace("\"", "");
      return num.toInt();
    }
  }
  return -1;
}

// =========================
// KIRIM STATUS ke Railway
// =========================
void sendStatusToServer()
{
  // JSON status (dashboard baca ini untuk indikator RUN/EMERGENCY)
  String out = "{";
  out += "\"type\":\"status\",";
  out += "\"count\":" + String(counter) + ",";
  out += "\"pwm\":" + String((int)pwmValue) + ",";
  out += "\"emergency\":" + String(emergencyStop ? 1 : 0);
  out += "}";

  wsClient.send(out);
}

// =========================
// CONNECT WS
// =========================
void connectWebsocket()
{
  if (wsConnected)
    return;

  Serial.println("WS: connecting to Railway...");

  // supaya gampang (tanpa CA cert). Untuk produksi lebih aman pakai sertifikat.
  wsClient.setInsecure();

  wsClient.onEvent([](WebsocketsEvent event, String data)
                   {
                     if (event == WebsocketsEvent::ConnectionOpened)
                     {
                       wsConnected = true;
                       Serial.println("WS: connected");
                     }
                     else if (event == WebsocketsEvent::ConnectionClosed)
                     {
                       wsConnected = false;
                       Serial.println("WS: disconnected");
                     }
                     else if (event == WebsocketsEvent::GotPing)
                     {
                       // optional
                     }
                     else if (event == WebsocketsEvent::GotPong)
                     {
                       // optional
                     } });

  wsClient.onMessage([](WebsocketsMessage message)
                     {
                       String msg = message.data();
                       msg.trim();

                       Serial.print("WS RX: ");
                       Serial.println(msg);

                       int v = parsePwmFromMessage(msg);
                       if (v >= 0 && v <= 255)
                       {
                         pwmValue = v;

                         // safety priority: kalau emergency, PWM disimpan tapi motor tetap stop
                         if (!emergencyStop)
                           motorForward(pwmValue);

                         Serial.print("PWM set via WS = ");
                         Serial.println((int)pwmValue);
                       } });

  bool ok = wsClient.connect(WS_URL);
  if (!ok)
  {
    wsConnected = false;
    Serial.println("WS: connect FAILED");
  }
}

// =========================
// SETUP
// =========================
void setup()
{
  Serial.begin(115200);
  delay(300);

  // IR
  pinMode(IR_PIN, INPUT_PULLUP);

  // motor pins
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);

  ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENA_PIN, PWM_CH);

  // LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  // ===== WiFi connect (butuh internet untuk Railway) =====
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  int ch = WiFi.channel();
  Serial.print("WiFi Channel: ");
  Serial.println(ch);
  Serial.print("NodeB MAC: ");
  Serial.println(WiFi.macAddress());

  // ===== ESP-NOW init (pakai channel WiFi router) =====
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

  // start motor
  motorForward(pwmValue);
  lcdShowRunTemplate();
  lcdUpdateRunValues();

  // connect websocket
  connectWebsocket();

  Serial.println("READY: ESP-NOW + Railway WS + IR + LCD + PWM");
  Serial.println("NOTE: NodeA channel harus sama dengan WiFi Channel di atas!");
}

// =========================
// LOOP
// =========================
void loop()
{
  // poll websocket
  wsClient.poll();

  // reconnect WS jika putus
  if (!wsConnected && (millis() - lastWsReconnectMs >= WS_RECONNECT_INTERVAL))
  {
    lastWsReconnectMs = millis();
    connectWebsocket();
  }

  // kirim status berkala (RUN/EMERGENCY + count + pwm)
  if (wsConnected && (millis() - lastStatusSendMs >= STATUS_INTERVAL))
  {
    lastStatusSendMs = millis();
    sendStatusToServer();
  }

  // update LCD saat mode berubah
  if (emergencyStop != lastEmergencyStop)
  {
    lastEmergencyStop = emergencyStop;

    if (emergencyStop)
      lcdShowEmergency();
    else
    {
      lcdShowRunTemplate();
      lcdUpdateRunValues();
      motorForward(pwmValue);
    }
  }

  // safety priority
  if (emergencyStop)
  {
    delay(10);
    return;
  }

  // IR counting (edge detect + lockout)
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

  // LCD periodic update
  if (millis() - lastLcdMs >= LCD_INTERVAL)
  {
    lastLcdMs = millis();
    lcdUpdateRunValues();
  }
}