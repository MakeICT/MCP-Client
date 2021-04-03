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

static const char *WIFI_TAG = "MCP_WIFI";


/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

// static esp_err_t wifi_event_handler(void *ctx, system_event_t *event);

int wifiInit(char* ssid, char* pass);

uint8_t wifiInit();
uint8_t wifiStart();
uint8_t wifiStop();
uint8_t wifiRestart();
bool wifiIsConnected();

#endif