#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define BUTTON_PIN 4

// GANTI sesuai MAC NodeB
uint8_t receiverMac[] = {0xDC, 0xB4, 0xD9, 0x1B, 0xB3, 0x18};

// Samakan dengan channel WiFi NodeB
#define ESPNOW_CHANNEL 6

typedef struct __attribute__((packed))
{
  uint8_t emergency;
} EspNowMsg;

volatile bool buttonIrqFlag = false;
volatile unsigned long buttonIrqMs = 0;

bool emergencyState = false;
const unsigned long BUTTON_DEBOUNCE_MS = 120;

void IRAM_ATTR onButtonPressed()
{
  unsigned long now = millis();
  if (now - buttonIrqMs >= BUTTON_DEBOUNCE_MS)
  {
    buttonIrqMs = now;
    buttonIrqFlag = true;
  }
}

void onSend(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.print("Send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

void sendEmergency(uint8_t emg)
{
  EspNowMsg msg;
  msg.emergency = emg;

  esp_err_t result = esp_now_send(receiverMac, (uint8_t *)&msg, sizeof(msg));
  if (result == ESP_OK)
  {
    Serial.print("Sent EMERGENCY=");
    Serial.println(emg);
  }
  else
  {
    Serial.print("Send error: ");
    Serial.println(result);
  }
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  delay(50);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  Serial.print("NodeA MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("ESP-NOW channel: ");
  Serial.println(ESPNOW_CHANNEL);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW init FAILED!");
    while (true)
      delay(100);
  }

  esp_now_register_send_cb(onSend);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMac, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Add peer FAILED!");
    while (true)
      delay(100);
  }

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonPressed, FALLING);

  Serial.println("NodeA Ready");
  Serial.println("Push button interrupt active");
}

void loop()
{
  if (buttonIrqFlag)
  {
    noInterrupts();
    buttonIrqFlag = false;
    interrupts();

    emergencyState = !emergencyState;

    Serial.print("BUTTON -> toggle, emergencyState=");
    Serial.println(emergencyState ? "1 (STOP)" : "0 (RUN)");

    sendEmergency(emergencyState ? 1 : 0);
  }
}