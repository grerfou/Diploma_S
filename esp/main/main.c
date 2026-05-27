/*******************************************************************************
*
*   ESP32-S3 Hub — main.c (ESP-IDF)
*
*   Reçoit les données de plusieurs senders ESP32 via ESP-NOW
*   et les redirige vers le PC via Serial (UART0).
*
*   Format envoyé au PC : "NOM:VALEUR\n"
*   Exemple : "PINK:1.0000\n" ou "BLUE:0.7500\n"
*
*******************************************************************************/

// ─────────────────────────────────────────
// MODE : décommenter UN seul des deux
// ─────────────────────────────────────────
//#define USE_BOOL
#define USE_FLOAT

// ─────────────────────────────────────────
// Includes
// ─────────────────────────────────────────

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_random.h"

// ─────────────────────────────────────────
// Config
// ─────────────────────────────────────────

#define TAG             "HUB"
#define MAX_SENDERS     5
#define UPDATE_RATE_MS  40

// ─────────────────────────────────────────
// Struct — doit être identique au sender
// ─────────────────────────────────────────

typedef struct {
    char  name[32];
    bool  activation_state;
    float proximity;
} struct_message;

// ─────────────────────────────────────────
// État interne des senders
// ─────────────────────────────────────────

typedef struct {
    char  name[32];
    float value;
    bool  known;
} SenderState;

static SenderState s_senders[MAX_SENDERS];
static int         s_senderCount = 0;

// ─────────────────────────────────────────
// Trouver ou enregistrer un sender
// ─────────────────────────────────────────

static SenderState* getSender(const char *name) {
    for (int i = 0; i < s_senderCount; i++) {
        if (strncmp(s_senders[i].name, name, 32) == 0) {
            return &s_senders[i];
        }
    }
    if (s_senderCount < MAX_SENDERS) {
        SenderState *s = &s_senders[s_senderCount++];
        strncpy(s->name, name, 32);
        s->value = 0.0f;
        s->known  = true;
        ESP_LOGI(TAG, "Nouveau sender : %s", name);
        return s;
    }
    ESP_LOGW(TAG, "MAX_SENDERS atteint !");
    return NULL;
}

// ─────────────────────────────────────────
// Traitement d'un message (réel ou simulé)
// ─────────────────────────────────────────

static void processMessage(const struct_message *msg) {
    SenderState *sender = getSender(msg->name);
    if (!sender) return;

#ifdef USE_BOOL
    sender->value = msg->activation_state ? 1.0f : 0.0f;
#endif

#ifdef USE_FLOAT
    sender->value = msg->activation_state ? msg->proximity : 0.0f;
#endif

    // Envoie au PC via UART0
    printf("%s:%.4f\n", sender->name, sender->value);
}

// ─────────────────────────────────────────
// Callback réception ESP-NOW
// ─────────────────────────────────────────

static void onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len != sizeof(struct_message)) {
        ESP_LOGW(TAG, "Paquet inattendu (%d bytes)", len);
        return;
    }
    struct_message msg;
    memcpy(&msg, data, sizeof(msg));
    processMessage(&msg);
}

// ─────────────────────────────────────────
// Init WiFi (requis pour ESP-NOW)
// ─────────────────────────────────────────

static void initWifi(void) {
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    ESP_LOGI(TAG, "WiFi STA démarré");
}

// ─────────────────────────────────────────
// Init ESP-NOW
// ─────────────────────────────────────────

static void initESPNow(void) {
    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW init failed");
        esp_restart();
    }
    esp_now_register_recv_cb(onDataReceived);
    ESP_LOGI(TAG, "ESP-NOW OK");
}

// ─────────────────────────────────────────
// app_main
// ─────────────────────────────────────────

void app_main(void) {
    // Init NVS (requis par WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Init senders
    memset(s_senders, 0, sizeof(s_senders));
    s_senderCount = 0;

    initWifi();
    initESPNow();

    ESP_LOGI(TAG, "Hub prêt");

    // ─────────────────────────────────────
    // Boucle principale — simulation
    // Commenter ce bloc quand les vrais senders sont branchés
    // ─────────────────────────────────────
    while (1) {
        // ── SIMULATION ────────────────────────────────────────────
        struct_message fake_pink;
        strncpy(fake_pink.name, "PINK", sizeof(fake_pink.name));
        fake_pink.activation_state = (esp_random() % 2 == 0);
        fake_pink.proximity        = fake_pink.activation_state
                                     ? ((float)(esp_random() % 1000) / 1000.0f)
                                     : 0.0f;

        struct_message fake_blue;
        strncpy(fake_blue.name, "BLUE", sizeof(fake_blue.name));
        fake_blue.activation_state = (esp_random() % 2 == 0);
        fake_blue.proximity        = fake_blue.activation_state
                                     ? ((float)(esp_random() % 1000) / 1000.0f)
                                     : 0.0f;

        processMessage(&fake_pink);
        processMessage(&fake_blue);
        // ── FIN SIMULATION ────────────────────────────────────────

        vTaskDelay(pdMS_TO_TICKS(UPDATE_RATE_MS));
    }
}
