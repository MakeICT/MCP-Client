#include "mcp_websocket.h"

const char *WS_TAG = "MCP_WEBSOCKET";

QueueHandle_t ws_send_queue;
QueueHandle_t ws_receive_queue;


/* FreeRTOS event group to signal when we are connected & ready to make a request */
const int WS_CONNECTED_BIT = BIT0;
static EventGroupHandle_t ws_event_group;

esp_websocket_client_handle_t client;

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
    ws_event_group = xEventGroupCreate();

    esp_websocket_client_config_t websocket_cfg = {};

    websocket_cfg.uri = "ws://10.0.0.134:5000/clients/socket";

    ESP_LOGI(WS_TAG, "Connecting to %s...", websocket_cfg.uri);

    client = esp_websocket_client_init(&websocket_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);

    ws_send_queue = xQueueCreate(10, sizeof(char*));
    ws_receive_queue = xQueueCreate(10, sizeof(char*));
}

void websocket_start(void)
{
    esp_websocket_client_start(client);
    xEventGroupWaitBits(ws_event_group, WS_CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(WS_TAG, "Websocket Started");
}

void websocket_stop()
{
    esp_websocket_client_stop(client);
    esp_websocket_client_destroy(client);
    ESP_LOGI(WS_TAG, "Websocket Stopped");
}

void websocket_send(char * data, int len)
{   
    if (esp_websocket_client_is_connected(client)) {
        ESP_LOGI(WS_TAG, "Sending %s", data);
        esp_websocket_client_send(client, data, len, portMAX_DELAY);
    }
}

char* websocket_receive()
{
    char* data;
    if(ws_receive_queue == NULL){
        ESP_LOGE(WS_TAG, "Receive queue does not exist!");
        return NULL;
    }
    else {
        xQueueReceive(ws_receive_queue, &data, portMAX_DELAY);
        ESP_LOGI(WS_TAG, "websocket_receive(): %s", data);

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
