#include "mcp_websocket.h"

const char *WS_TAG = "MCP_WEBSOCKET";

QueueHandle_t ws_send_queue = NULL;
QueueHandle_t ws_receive_queue = NULL;

uint64_t ws_last_response_received = 0;


/* FreeRTOS event group to signal when we are connected & ready to make a request */
const int WS_CONNECTED_BIT = BIT0;
static EventGroupHandle_t ws_event_group = NULL;

esp_websocket_client_handle_t client = NULL;

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(WS_TAG, "WEBSOCKET_EVENT_CONNECTED");
        xEventGroupSetBits(ws_event_group, WS_CONNECTED_BIT);
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(WS_TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        xEventGroupClearBits(ws_event_group, WS_CONNECTED_BIT);
        break;
    case WEBSOCKET_EVENT_DATA: 
    {
        ws_last_response_received = esp_timer_get_time();
        ESP_LOGI(WS_TAG, "WEBSOCKET_EVENT_DATA");
        ESP_LOGI(WS_TAG, "Received opcode=%d", data->op_code);
        if(data->data_len > 0) {
            ESP_LOGW(WS_TAG, "Received=%.*s", data->data_len, (char *)data->data_ptr);
            ESP_LOGW(WS_TAG, "Total payload length=%d, data_len=%d, current payload offset=%d\r\n", data->payload_len, data->data_len, data->payload_offset);

            char *rcv_data = (char*) pvPortMalloc(data->data_len + 1);
            strncpy(rcv_data, (char *)data->data_ptr, data->data_len);
            // memcpy(rcv_data, (char *)data->data_ptr, data->data_len);
            rcv_data[data->data_len] = '\0';

            xQueueSend(ws_receive_queue, (void*) &rcv_data, (TickType_t) 0);
        }

        break;
    }
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(WS_TAG, "WEBSOCKET_EVENT_ERROR");
        break;
    }
}

void websocket_receive_task()
{
    while(1)
    {
        char *msg = NULL;
        xQueueReceive(ws_receive_queue, msg, portMAX_DELAY);

        ESP_LOGI(WS_TAG, "[message] %s", msg);
    }
}

void websocket_init()
{
    if(ws_event_group == NULL)
        ws_event_group = xEventGroupCreate();

    esp_websocket_client_config_t websocket_cfg = {};

    websocket_cfg.uri = "ws://" CONFIG_SERVER ":80/clients/socket";

    ESP_LOGI(WS_TAG, "Connecting to %s...", websocket_cfg.uri);

    if(client != NULL) {
        esp_websocket_client_stop(client);
        esp_websocket_client_destroy(client);
    }

    client = esp_websocket_client_init(&websocket_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);

    if(ws_send_queue == NULL)
        ws_send_queue = xQueueCreate(10, sizeof(char*));
    if(ws_receive_queue == NULL)
        ws_receive_queue = xQueueCreate(10, sizeof(char*));
}

bool websocket_start(void)
{
    esp_err_t err = esp_websocket_client_start(client);
    xEventGroupWaitBits(ws_event_group, WS_CONNECTED_BIT, false, true, 10000 / portTICK_PERIOD_MS);
    if(err == ESP_OK)
    {
        ESP_LOGI(WS_TAG, "Websocket Started");
        return true;
    }
    else 
    {
        ESP_LOGE(WS_TAG, "Websocket failed to start!");
        return false;
    }
}

void websocket_stop()
{
    esp_websocket_client_stop(client);
    // esp_websocket_client_destroy(client);
    ESP_LOGI(WS_TAG, "Websocket Stopped");
}

void websocket_send(char * data, int len)
{   
    if (esp_websocket_client_is_connected(client)) {
        ESP_LOGI(WS_TAG, "Sending %s", data);
        esp_websocket_client_send(client, data, len, 10000 / portTICK_PERIOD_MS);
    }
}

char* websocket_receive()
{
    char* data = NULL;
    if(ws_receive_queue == NULL){
        ESP_LOGE(WS_TAG, "Receive queue does not exist!");
        return NULL;
    }
    else {
        bool ret = xQueueReceive(ws_receive_queue, &data, 10000 / portTICK_PERIOD_MS);
        if(ret) 
            ESP_LOGI(WS_TAG, "websocket_receive(): %s", data);
        else{
            ESP_LOGW(WS_TAG, "websocket_receive(): NO DATA");
        }
        return data;
    }
}

char* websocket_send_and_receive(char * data, int len)
{   
    websocket_send(data, len);

    return websocket_receive();
}

bool websocket_connected()
{
    return esp_websocket_client_is_connected(client);
}

uint64_t websocket_idle_time()
{
    uint32_t idle_time = esp_timer_get_time() - ws_last_response_received;
    ESP_LOGI(WS_TAG, "Idle time: %d", idle_time);
    return idle_time;
}
