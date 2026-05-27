#include <esp_now.h>
#include <WiFi.h>


typedef struct struct_message {
    char name[32];
    bool activation_state ;
} struct_message;

struct_message data_received;


void init_ESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }
}

// callback function that will be executed when data is received
void data_reception(const esp_now_recv_info* mac_addr, const unsigned char* data, int len) {
  memcpy(&data_received, data, sizeof(data_received));
  Serial.print("Bytes received: "); // ca ca degagera DEBUG
  Serial.println(len);

  Serial.print("Source name: "); // ca aussi t'aura juste les données en dessous
  Serial.println(data_received.name); //char[32] (enfait les types sont dans le struct tout en haut)

  Serial.print("State: "); // et la aussi mais tu t'en doutais deja
  Serial.println(data_received.activation_state); //boolean pour l'instant
}

// void configDeviceAP() {
//   char* SSID = "Slave_1";
//   bool result = WiFi.softAP(SSID, "Slave_1_Password", CHANNEL, 0);
//   if (!result) {
//     Serial.println("AP Config failed.");
//   } else {
//     Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
//   }
// }
 
void setup() {
  Serial.begin(115200);
  
  WiFi.mode(WIFI_STA);
  // configDeviceAP();
  init_ESPNow();
  esp_now_register_recv_cb(data_reception);
}
 
void loop() {
//si tu veux simuler tu appelles la fonction data_reception
}