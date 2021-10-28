#include "mcp_link.h"

static const char *LINK_TAG = "MCP_LINK";

TaskHandle_t heartbeat_task_handle = NULL;

static void heartbeat_task(void *arg)
{
    UBaseType_t uxHighWaterMark = 0;

    uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    ESP_LOGI(LINK_TAG, "Task stack high water mark: %d", uxHighWaterMark);
    
    while(1)
    {
        ESP_LOGI(LINK_TAG, "Heartbeat task");
        if(websocket_connected()) {
            char message[128];

            int len = sprintf(message, "{\"message\":\"heartbeat\", \"client_id\": 1}");
            
            char *data = websocket_send_and_receive(message, len);

            if(websocket_idle_time() > 60000000) {
                ESP_LOGE(LINK_TAG, "TIMEOUT!");
                websocket_stop();
            }
            
            free(data);
        }
        printf("Free heap: %d\n", xPortGetFreeHeapSize());
        vTaskDelay(30000 / portTICK_PERIOD_MS);

        uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
        ESP_LOGI(LINK_TAG, "Task stack high water mark: %d", uxHighWaterMark);
    }
    vTaskDelete(NULL);
}

MCPLink::MCPLink()
{
}

bool MCPLink::ConnectWebsocket()
{
    websocket_init();
    bool started = websocket_start();
    if(started) {
        if(heartbeat_task_handle != NULL)
            vTaskDelete(heartbeat_task_handle);
        xTaskCreate(heartbeat_task, "heartbeat", 4096, NULL, 10, &heartbeat_task_handle);
    }
    return started;
}

bool MCPLink::WebsocketConnected()
{
    return websocket_connected();
}

int MCPLink::AuthenticateNFC(char* nfc_id) {
	int status = 0;

    char format[] = "clients/%d/verify/%s";
    char payload[] = "{}";
    uint8_t endpoint_length = sizeof(char) * (strlen(format) + strlen(nfc_id) + 3);
    ESP_LOGI(LINK_TAG, "endpoint length: %d", endpoint_length);
    char* endpoint = (char*) malloc(endpoint_length);

    sprintf(endpoint, "clients/%d/verify/%s", client_id, nfc_id);

    char *data;

    if(WebsocketConnected()){
        char message[128];

        int len = sprintf(message, "{\"message\":\"verify\", \"client_id\": %d, \"nfc_id\":\"%s\"}", client_id, nfc_id);
        
        data = websocket_send_and_receive(message, len);
    }
    else {
        data = api_call(endpoint, payload); 
    }

    if(data==NULL){
        ESP_LOGE(LINK_TAG, "ERROR authenticating NFC!");
    	return -1;
    }

    if(data)  {
        cJSON *root = cJSON_Parse(data);

        char *authorized = cJSON_GetObjectItem(root, "authorized")->valuestring;
        if(strcmp(authorized, "true") == 0) {
            status = 1;
            ESP_LOGI(LINK_TAG, "Card Accepted!");
        } 
        else {
            ESP_LOGI(LINK_TAG, "Card Denied!");
        }

        cJSON_Delete(root);
        vPortFree(data);
    }

    free(endpoint);

    return status;
}