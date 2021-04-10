#ifndef MCP_WEBSOCKET
#define MCP_WEBSOCKET


#include <stdio.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
// #include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_websocket_client.h"
#include "esp_event.h"
#include "esp_event_loop.h"

#define NO_DATA_TIMEOUT_SEC 10

extern const char *WS_TAG;

void websocket_init();
void websocket_start(void);
void websocket_stop();
void websocket_send(char * data, int len);
bool websocket_connected();
char* websocket_send_and_receive(char * data, int len);

#endif