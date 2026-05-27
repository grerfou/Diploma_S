#include <esp_now.h>
#include <WiFi.h>

#define NAME "PINK\0"
#define UPDATE_RATE 40  //millis

uint8_t broadcastAddress[] = { 0x68, 0xB6, 0xB3, 0x3D, 0x6E, 0xD0 };

typedef struct struct_message {
  char name[32];
  bool activation_state;
} struct_message;

struct_message data_to_send;

esp_now_peer_info_t peerInfo;

void OnDataSent(const wifi_tx_info_t*mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void init_ESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  } else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  init_ESPNow();

  esp_now_register_send_cb(OnDataSent);

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void loop() {
  // Set values to send
  strcpy(data_to_send.name, NAME);
  data_to_send.activation_state = (random(2)>.5)? true : false;


  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&data_to_send, sizeof(data_to_send));

  if (result == ESP_OK) {
    Serial.println("Sent with success");
  } else {
    Serial.println("Error sending the data");
  }
   delay(UPDATE_RATE);
}