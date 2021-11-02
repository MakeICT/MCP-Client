#include "mcp_wifi.h"

const char *WIFI_TAG = "MCP_WIFI";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;


esp_netif_t *wifi_netif;
static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 5) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
        } else {
            // xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGW(WIFI_TAG, "failed to connect, sleeping 5 seconds");
			vTaskDelay(5000 / portTICK_PERIOD_MS);
			esp_wifi_connect();
			s_retry_num=0;
        }
        ESP_LOGI(WIFI_TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }
}

int wifiInit(char* ssid, char* pass)
{
    // tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();
    // ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL) );
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {};
    strcpy((char*) wifi_config.sta.ssid, (char*) ssid);
    strcpy((char*) wifi_config.sta.password, (char*) pass);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK,
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;


    ESP_LOGI(WIFI_TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    
    wifi_netif = esp_netif_create_default_wifi_sta();
    
    return 0;
}

int wifi_set_static_ip(const char* ip, const char* gw, const char* nm)
{
    esp_netif_dhcpc_stop(wifi_netif);

    esp_netif_ip_info_t ip_info;

    // IP4_ADDR(&ip_info.ip, 10, 0, 0, 250); 
    esp_netif_str_to_ip4(ip, &ip_info.ip); 
   	esp_netif_str_to_ip4(gw, &ip_info.gw);
   	esp_netif_str_to_ip4(nm, &ip_info.netmask);

    esp_netif_set_ip_info(wifi_netif, &ip_info);

    return 0;
}

uint8_t wifiStart()
{
    ESP_ERROR_CHECK( esp_wifi_start() );

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,false, true, portMAX_DELAY);

    // if (!(xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT)) {
    //     // indicate failure
    //     return 1;
    // }

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