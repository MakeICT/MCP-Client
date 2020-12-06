#ifndef BINGO_API_H
#define BINGO_API_H

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
// #include "freertos/heap_4.c"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include <cstring>

#include <esp_http_client.h>
#include "mbedtls/md.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include <cJSON.h>

// #include "hmac_calc.h"

static const char *API_TAG = "MCP_API";

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER CONFIG_SERVER ":" CONFIG_PORT
#define WEB_URL "http://" WEB_SERVER
 
// #define WEB_SERVER CONFIG_SERVER ":" CONFIG_PORT
// #define WEB_URL CONFIG_SERVER

uint8_t client_id = 1;
QueueHandle_t response_queue;

esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(API_TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(API_TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(API_TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(API_TAG, "HTTP_EVENT_ON_HEADER");
            printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(API_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // if (!esp_http_client_is_chunked_response(evt->client)) {
            //     printf("%.*s", evt->data_len, (char*)evt->data);
            // }
                printf("%.*s\n", evt->data_len, (char*)evt->data);

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(API_TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(API_TAG, "HTTP_EVENT_DISCONNECTED");
            break;

        default:
            break;
    }
    return ESP_OK;
}

typedef struct {
    char endpoint[64] = {'\0'};
    char post_data[300] = {'\0'};
} api_call_data_t;

static void http_api_task(void *pvParameters)
{
    api_call_data_t data = *(api_call_data_t*) pvParameters;
    char* url{ new char[strlen(data.endpoint) + strlen(WEB_URL "/api/") + 1]{'\0'}};
    
    esp_err_t err = ESP_OK;
    do {    
        // client_init();
        esp_http_client_handle_t client;
        
        esp_http_client_config_t config = {
        };

        config.event_handler = _http_event_handle;
        config.url = WEB_URL;
        // config.buffer_size = 2048;
        // config.timeout_ms = 2000;


        client = esp_http_client_init(&config);
        // esp_http_client_set_header(client, "Connection", "keep-alive");



        sprintf(url, "%s/api/%s", WEB_URL, data.endpoint);
        esp_http_client_set_url(client, url);
        // ESP_LOGI(API_TAG, "STRING LENGTH: %d : %s", strlen(data.endpoint) + strlen(CONFIG_SERVER "/api/") + 1, url);

        esp_http_client_set_method(client, HTTP_METHOD_GET);
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, data.post_data, strlen(data.post_data));


        err = esp_http_client_perform(client);
        char buffer[2048] = {'\0'};
        // ESP_LOGI(API_TAG, "Length: %d", esp_http_client_read(client, buffer, 2048));
        // ESP_LOGI(API_TAG, "Content: %s", buffer);

        if (err == ESP_OK) {
            if (esp_http_client_get_status_code(client) != 200)
                ESP_LOGD(API_TAG, "Status = %d", esp_http_client_get_status_code(client));
            else
            {
                uint8_t content_length = esp_http_client_get_content_length(client);

                ESP_LOGI(API_TAG, "Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));

                // char* data = (char*) malloc(content_length) + 1;
                char* data = (char*) pvPortMalloc(content_length + 1);
                esp_http_client_read(client, data, content_length);
                data[content_length] = '\0';
                ESP_LOGI(API_TAG, "Data: %s", data);

                xQueueSend(response_queue, (void*) &data, (TickType_t) 0);
            }
        }
        else {
            ESP_LOGE(API_TAG, "Error %d %s", err, esp_err_to_name(err));
            if (err == ESP_FAIL)
                ESP_LOGE(API_TAG, "ESP_FAIL");
            }
        esp_http_client_close(client);
        esp_http_client_cleanup(client);        
    } while(err != ESP_OK);

    delete[] url;
    free(pvParameters);
    vTaskDelete(NULL);
}


char* api_call(const char* endpoint, char* payload) {
    // char signature[65] = {'\0'};
    // calculate_hmac(CONFIG_SECRET_KEY, payload, signature);


    api_call_data_t* data = (api_call_data_t*) malloc(sizeof(api_call_data_t));

    // api_call_data_t data;
    strcpy(data->endpoint, endpoint);
    strcpy(data->post_data, payload);
    // if(strlen(payload) < 3)
    //     sprintf(data->post_data, "{\"signature\":\"%s\"}",  signature);
    // else {
    //     payload[strlen(payload)-1] = ',';
    //     sprintf(data->post_data,
    //         "%s"
    //         "\"signature\":\"%s\"}" 
    //         , payload, signature);    
    // }

    response_queue = xQueueCreate(1, sizeof(char*));

    ESP_LOGI(API_TAG, "%s", data->endpoint);

    xTaskCreate(&http_api_task, "http_api_task", 8192, (void*) data, 5, NULL);

    char* response;
    if(xQueueReceive(response_queue, &response, (TickType_t) 1000 )) {
        ESP_LOGI(API_TAG, "API Call Response: %s", response);
        return response;
    }
    else {
        ESP_LOGI(API_TAG, "API Call did not return in time");
    }

    return NULL;
}

bool authenticate_nfc(char* nfc_id) {
    bool status = false;

    char format[] = "clients/%d/verify/%s";
    char payload[] = "{}";
    uint8_t endpoint_length = sizeof(char) * (strlen(format) + strlen(nfc_id) + 3);
    ESP_LOGI(API_TAG, "endpoint length: %d", endpoint_length);
    char* endpoint = (char*) malloc(endpoint_length);

    sprintf(endpoint, "clients/%d/verify/%s", client_id, nfc_id);

    char* data = api_call(endpoint, payload); 

    if(data)  {
        cJSON *root = cJSON_Parse
        (data);
        char *authorized = cJSON_GetObjectItem(root, "authorized")->valuestring;
        if(strcmp(authorized, "true") == 0) {
            status = true;
            ESP_LOGI(API_TAG, "Card Accepted!");
        } 
        else {
            ESP_LOGI(API_TAG, "Card Denied!");
        }

        cJSON_Delete(root);
        vPortFree(data);
    }

    free(endpoint);

    return status;
}

#endif