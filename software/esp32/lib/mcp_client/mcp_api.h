#ifndef MCP_API_H
#define MCP_API_H

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_http_client.h>

#include <cJSON.h>

extern const char *API_TAG;

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER CONFIG_SERVER ":" CONFIG_PORT
#define WEB_URL "http://" WEB_SERVER
#define CLIENT_TAG CONFIG_CLIENT_TAG


extern int client_id;
extern QueueHandle_t request_queue;
extern QueueHandle_t response_queue;

extern char* end_flag;

typedef struct {
    esp_http_client_method_t method = HTTP_METHOD_GET;
    char endpoint[32] = {'\0'};
    char post_data[500] = {'\0'};
    char *response;
    SemaphoreHandle_t complete;
} api_call_data_t;

esp_err_t _http_event_handle(esp_http_client_event_t *evt);

void http_api_task(void *pvParameters);
char* api_call(const char* endpoint, char* payload);

int authenticate_nfc(char* nfc_id); 
bool load_nfc_list(); 

#endif