#include "mcp_api.h"

const char *API_TAG = "MCP_API";

int client_id = CLIENT_ID;
const char* end_flag = "[[[HTTP_END]]]";
QueueHandle_t request_queue;
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
        case HTTP_EVENT_ON_DATA: {
            ESP_LOGD(API_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // if (!esp_http_client_is_chunked_response(evt->client)) {
            //     printf("%.*s", evt->data_len, (char*)evt->data);
            // }
            // printf("%.*s\n", evt->data_len, (char*)evt->data);
            char* data = (char*) pvPortMalloc(evt->data_len + 1);
            // esp_http_client_read(client, data, content_length);
            strncpy(data, (char*) evt->data, evt->data_len);
            data[evt->data_len] = '\0';
            xQueueSend(response_queue, (void*) &data, (TickType_t) 0);

            break;
        }
        case HTTP_EVENT_ON_FINISH: {
            ESP_LOGD(API_TAG, "HTTP_EVENT_ON_FINISH");
        
            char* data = (char*) pvPortMalloc(strlen(end_flag) + 1);
            // esp_http_client_read(client, data, content_length);
            strncpy(data, end_flag, strlen(end_flag));
            data[strlen(end_flag)] = '\0';
            xQueueSend(response_queue, (void*) &data, (TickType_t) 0);
            break;
        }
        
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(API_TAG, "HTTP_EVENT_DISCONNECTED");
            break;

        default:
            break;
    }
    return ESP_OK;
}

void http_api_task(void *pvParameters)
{
    response_queue = xQueueCreate(10, sizeof(char*));
    request_queue = xQueueCreate(10, sizeof(api_call_data_t*));
    
    while(1) {
        api_call_data_t *data;
        xQueueReceive(request_queue, &data, portMAX_DELAY);

        static int retry_delay;
        retry_delay = 500;
        uint8_t attempt = 1;
        bool success = false;

        while(!success) {
            // client_init();
            esp_http_client_handle_t client;
            
            esp_http_client_config_t config = {
            };

            config.event_handler = _http_event_handle;
            // config.cert_pem = server_root_cert_pem;
            config.url = CONFIG_SERVER;
            config.buffer_size = 10240;
            config.timeout_ms = 1000;


            client = esp_http_client_init(&config);
            // esp_http_client_set_header(client, "Connection", "keep-alive");

            char* url{ new char[strlen(data->endpoint) + strlen(CONFIG_SERVER "/api/") + 1]{'\0'}};
            sprintf(url, "%s/api/%s", CONFIG_SERVER, data->endpoint);

            if (url != NULL) ESP_LOGI(API_TAG, "URL = %s", url);
            if (client == NULL) ESP_LOGE(API_TAG, " << NULL CLIENT! >>");

            esp_http_client_set_url(client, url);
            // ESP_LOGI(API_TAG, "STRING LENGTH: %d : %s", strlen(data.endpoint) + strlen(CONFIG_SERVER "/api/") + 1, url);

            esp_http_client_set_method(client, data->method);
            esp_http_client_set_header(client, "Content-Type", "application/json");
            esp_http_client_set_post_field(client, data->post_data, strlen(data->post_data));

            if (attempt > 1) {
                ESP_LOGW(API_TAG, "attempt: %d\n", attempt);
            }
            else {
                ESP_LOGD(API_TAG, "attempt: %d\n", attempt);
            }
            esp_err_t err = esp_http_client_perform(client);
            // ESP_LOGI(API_TAG, "Length: %d", esp_http_client_read(client, buffer, 2048));
            // ESP_LOGI(API_TAG, "Content: %s", buffer);
        
            if (err == ESP_OK) { 
                char *response, *full_response;
                uint8_t chunk_count = 0;
                char* chunks[10];
                int resp_len = 0;
                
                if (esp_http_client_get_status_code(client) != 200)
                    ESP_LOGD(API_TAG, "Status = %d", esp_http_client_get_status_code(client));
                else
                {
                    ESP_LOGD(API_TAG, "Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
                }
                
                while(xQueueReceive(response_queue, &response, 10 / portTICK_PERIOD_MS)) {
                    if(strncmp(response, end_flag, strlen(end_flag)) == 0) {
                        ESP_LOGD(API_TAG, "Got termination flag\n");
                        vPortFree(response);
                        break;
                    }
                    chunks[chunk_count] = response;
                    chunk_count += 1;
                    resp_len += strlen(response);
                }
                if (chunk_count > 0) {
                    full_response = (char*) calloc(resp_len + 1, sizeof(char));
                    ESP_LOGD(API_TAG, "API Call Response Length: %d", resp_len);

                    for (int i=0; i<chunk_count; i++) {
                        snprintf(full_response, resp_len + 1, "%s%s", full_response, chunks[i]);
                        vPortFree(chunks[i]);
                    }
                    ESP_LOGI(API_TAG, "API Call Response: %s", full_response);

                    data->response = full_response;
                    xSemaphoreGive(data->complete);
                    success = true;
                }
                else if(chunk_count == 0) {
                    ESP_LOGE(API_TAG, "No API data received");
                }
        
            }
            else {
                ESP_LOGE(API_TAG, "Error %d %s", err, esp_err_to_name(err));
                if (err == ESP_FAIL)
                    ESP_LOGE(API_TAG, "ESP_FAIL");
            }
            attempt += 1;
            retry_delay *= 2;

            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            xQueueReset(response_queue);
            delete[] url;

            if (attempt == 5) {
                ESP_LOGE(API_TAG, "Failed to get a response after 5 attempts!");
                data->response = NULL;
                xSemaphoreGive(data->complete);
                break;
            }

            vTaskDelay(retry_delay / portTICK_PERIOD_MS);
        }
    }

    vTaskDelete(NULL);
}

char* api_call(const char* endpoint, char* payload) {
    // char signature[65] = {'\0'};
    // calculate_hmac(CONFIG_SECRET_KEY, payload, signature);


    api_call_data_t* data = (api_call_data_t*) malloc(sizeof(api_call_data_t));

    // api_call_data_t data;
    strcpy(data->endpoint, endpoint);
    strcpy(data->post_data, payload);
    data->method = HTTP_METHOD_GET; 
    // if(strlen(payload) < 3)
    //     sprintf(data->post_data, "{\"signature\":\"%s\"}",  signature);
    // else {
    //     payload[strlen(payload)-1] = ',';
    //     sprintf(data->post_data,
    //         "%s"
    //         "\"signature\":\"%s\"}"
    //         , payload, signature);    
    // }
    ESP_LOGI(API_TAG, "API Call Request: /%s, %s", endpoint, data->post_data);

    data->complete = xSemaphoreCreateBinary();

    xQueueSend(request_queue, (void*) &data, portMAX_DELAY);

    xSemaphoreTake(data->complete, portMAX_DELAY);

    char* response = data->response;
    vSemaphoreDelete(data->complete);
    ESP_LOGI(API_TAG, "api_call completed or failed\n");
    // printf("response: %s\n", response);
    free(data);

    return response;
}   

bool load_nfc_list() { //char* nfc_list
    bool status = false;

    char format[] = "clients/%d/get_nfc_list";
    char payload[] = "{}";
    uint8_t endpoint_length = sizeof(char) * (strlen(format) + 3);
    ESP_LOGI(API_TAG, "endpoint length: %d", endpoint_length);
    char* endpoint = (char*) malloc(endpoint_length);

    sprintf(endpoint, "clients/%d/get_nfc_list", client_id);

    char* data = api_call(endpoint, payload);

    if(data)  {
        cJSON *root = cJSON_Parse
        (data);
        char *authorized = cJSON_GetObjectItem(root, "authorized")->valuestring;
        if(strcmp(authorized, "true") == 0) {
            status = true;
            ESP_LOGI(API_TAG, "Card Accepted!");
            cJSON *authorized_keys = cJSON_GetObjectItem(root, "keys");

            for (int i=0; i < cJSON_GetArraySize(authorized_keys); i++) {
            	char* nfc_key = cJSON_GetArrayItem(authorized_keys,i)->valuestring;
            	ESP_LOGI(API_TAG,"%s",nfc_key);
            }

            cJSON_Delete(authorized_keys);
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