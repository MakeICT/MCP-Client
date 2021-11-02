#ifndef ESP_WIFI_H
#define ESP_WIFI_H

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_event.h"
#include "esp_log.h"

extern const char *WIFI_TAG;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
const int CONNECTED_BIT = BIT0;
// #define FAIL_BIT      BIT1

// static esp_err_t wifi_event_handler(void *ctx, system_event_t *event);

int wifiInit(char* ssid, char* pass);

uint8_t wifiInit();
uint8_t wifiStart();
uint8_t wifiStop();
uint8_t wifiRestart();
int wifi_set_static_ip(char* ip, char* gw, char* nm);
bool wifiIsConnected();

#endif