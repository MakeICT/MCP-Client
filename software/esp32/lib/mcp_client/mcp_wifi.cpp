#include "mcp_wifi.h"

const char *WIFI_TAG = "MCP_WIFI";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;


static int s_retry_num = 0;

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */

		if (s_retry_num < 5) {
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
	        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
		} else {
			//			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
			ESP_LOGI(WIFI_TAG, "failed to connect, sleeping 5 minutes");
			vTaskDelay(300000 / portTICK_PERIOD_MS);
			esp_wifi_connect();
			s_retry_num=0;

		}

        break;
    default:
        break;
    }
    return ESP_OK;
}

int wifiInit(char* ssid, char* pass)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    wifi_config_t wifi_config = {};
    strcpy((char*) wifi_config.sta.ssid, (char*) ssid);
    strcpy((char*) wifi_config.sta.password, (char*) pass);

    ESP_LOGI(WIFI_TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );

    return 0;
}

uint8_t wifiStart()
{
    ESP_ERROR_CHECK( esp_wifi_start() );

    if (!(xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT)) {
        // indicate failure
        return 1;
    }

    return 0;
}

uint8_t wifiStop()
{
    ESP_ERROR_CHECK( esp_wifi_stop() );

    return 0;
}

uint8_t wifiRestart()
{
    wifiStop();
    wifiStart();

    return 0;
}

bool wifiIsConnected()
{
    return xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT;
}