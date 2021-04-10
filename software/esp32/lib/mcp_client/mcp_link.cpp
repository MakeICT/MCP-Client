#include "mcp_link.h"

MCPLink::MCPLink()
{
}

void MCPLink::ConnectWebsocket()
{
    websocket_init();
    websocket_start();
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
    ESP_LOGI(API_TAG, "endpoint length: %d", endpoint_length);
    char* endpoint = (char*) malloc(endpoint_length);

    sprintf(endpoint, "clients/%d/verify/%s", client_id, nfc_id);

    char *data;

    if(WebsocketConnected()){
        char message[128];

        int len = sprintf(message, "{\"message\":\"verify\", \"client_id\": 1, \"nfc_id\":\"04d76b1a8f4980\"}");
        
        data = websocket_send_and_receive(message, len);
    }
    else {
        data = api_call(endpoint, payload); 
    }

    if(data==NULL){
        ESP_LOGE(API_TAG, "ERROR authenticating NFC!");
    	return -1;
    }

    if(data)  {
        cJSON *root = cJSON_Parse(data);

        char *authorized = cJSON_GetObjectItem(root, "authorized")->valuestring;
        if(strcmp(authorized, "true") == 0) {
            status = 1;
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