/* HTTPS GET Example using plain mbedTLS sockets
 *
 * Contacts the howsmyssl.com API via TLS v1.2 and reads a JSON
 * response.
 *
 * Adapted from the ssl_client1 example in mbedtls.
 *
 * Original Copyright (C) 2006-2016, ARM Limited, All Rights Reserved, Apache 2.0 License.
 * Additions Copyright (C) Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD, Apache 2.0 License.
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"
#include <driver/adc.h>
#include <esp_timer.h>
#include <driver/gpio.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

// #include "utils.h"
#include "../mcp_client/mcp_api_new.h"
#include <reader.h>
#include "sdkconfig.h"

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define WIFI_SSID "mywifissid"
*/
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASSWORD

#define SERVER CONFIG_SERVER
#define PORT CONFIG_PORT

#define CLIENT_TAG CONFIG_CLIENT_TAG

#define CURRENT_TIMEOUT CONFIG_CURRENT_TIMEOUT * 1000000

#define OLIMEX_REL1_PIN     32
#define OLIMEX_REL2_PIN     33
#define OLIMEX_BUT_PIN      34


#define USBUARTTX			01
#define USBUARTRX			03
#define CANTX				05
#define CANRX				35
#define ETH1				19
#define ETH2				20
#define ETH3				21
#define ETH4				22
#define ETH5				23
#define ETH6				24
#define ETH7				25
#define ETH8				26

#define UEXT3				04
#define UEXT4				36  //input door bell
#define UEXT5				16
#define UEXT6				13
#define UEXT7				15
#define UEXT8				02
#define UEXT9				14
#define UEXT10				17

#define LED_RED				UEXT6
#define LED_YELLOW			UEXT5
#define LED_GREEN			UEXT3

#define ALARM_ARM_INPUT  	OLIMEX_BUT_PIN
#define ALARM_STATE_INPUT   39
#define DOOR_BELL_INPUT		UEXT4
//#define ALARM_MOTION_INPUT
#define ALARM_ARM_RELAY     OLIMEX_REL2_PIN
#define ALARM_DISARM_RELAY  OLIMEX_REL1_PIN
#define DOOR_STRIKE_RELAY 	12

#define GPIO_INPUT_PIN_SEL  ((1ULL<<ALARM_STATE_INPUT) | (1ULL<<DOOR_BELL_INPUT)) //| (1ULL<<ALARM_MOTION_INPUT)
#define GPIO_INPUT2_PIN_SEL  ((1ULL<<ALARM_ARM_INPUT)) //| (1ULL<<ALARM_MOTION_INPUT)
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<ALARM_ARM_RELAY)  | (1ULL<< ALARM_DISARM_RELAY)  | (1ULL<< DOOR_STRIKE_RELAY) )
#define GPIO_OUTPUT_LED_PIN_SEL  ((1ULL<< LED_RED) | (1ULL<< LED_YELLOW) | (1ULL<< LED_GREEN))
//#define GPIO_OUTPUT_FLOAT_PIN_SEL  ((1ULL<<NFC_RESET))

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

/* Constants that aren't configurable in menuconfig */
// #define WEB_SERVER "security.makeict.org"
// #define WEB_SERVER "securitytest.makeict.org:4443"
// #define WEB_URL "https://securitytest.makeict.org:4443"

// #define WEB_SERVER CONFIG_SERVER ":" CONFIG_PORT
// #define WEB_URL "https://" WEB_SERVER
// #define AUTH_ENDPOINT WEB_URL "/api/login"

static const char *TAG = "wifi_client_main";

static const short STATE_UNKNOWN 		= 0;
static const short STATE_SYSTEM_START 	= 1;
static const short STATE_WAIT_CARD 		= 2;
static const short STATE_AUTHORIZING 	= 3;
static const short STATE_CARD_REJECT	= 4;
static const short STATE_UNLOCKING_DOOR = 5;
static const short STATE_UNLOCKED_DOOR 	= 6;

extern "C" {
  void app_main();
}

static int state = STATE_SYSTEM_START;
//int power_on_time;
//int current_detected_time;

Reader card_reader;
//Light red_light((gpio_num_t)17);
//Light yellow_light((gpio_num_t)18);
//Light green_light((gpio_num_t)19);
//Light machine_power((gpio_num_t)33);
//Light disarm_alarm((gpio_num_t)25);
//Switch power_switch((gpio_num_t)32);

// static const char *REQUEST = "POST " AUTH_ENDPOINT "?email=" CONFIG_USERNAME    "&password=" CONFIG_PASSWORD "\r\n"
//     "Host: " WEB_SERVER "\r\n"
//     "Content-Type: application/x-www-form-urlencoded\r\n"
//     "\r\n";


/* Letsencrypt root cert, taken from server_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
// extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
// extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

void init(void)
{
  card_reader.init();
  state = 0;
static int call_count=0;

static void led_handler_task(void* arg)
{
	call_count++;
	static bool blink=0;
	static int blinkOn=250;
	static int blinkOff=750;
	static int lastState=-1;
	static int sleepcount=0;
    for(;;) {
//		printf("%d:%d,",state, lastState);

    	if(state!=lastState){
    		printf("Changing state from %d to %d (%d)\n",lastState, state,call_count);

    	    gpio_set_level((gpio_num_t)LED_RED, 0);
    	    gpio_set_level((gpio_num_t)LED_YELLOW, 0);
    	    gpio_set_level((gpio_num_t)LED_GREEN, 0);

    		blink=0;
    		sleepcount=0;

    		switch(state){


    		case STATE_AUTHORIZING:
    			blinkOn=100;
    			blinkOff=100;
    			break;
    		case STATE_SYSTEM_START:
    			blinkOn=25;
    			blinkOff=150;
    			break;
    		case STATE_UNLOCKING_DOOR:
    			blinkOn=250;
    			blinkOff=250;
    			break;
    		case STATE_CARD_REJECT:
        	    gpio_set_level((gpio_num_t)LED_RED, 1);
        	    gpio_set_level((gpio_num_t)LED_GREEN, 0);
        	    gpio_set_level((gpio_num_t)LED_YELLOW, 0);
        	    vTaskDelay(1500 / portTICK_RATE_MS);
    			break;
    		case STATE_WAIT_CARD:
    			blinkOn=50;
    			blinkOff=1000;
				break;
    		default:
    			blinkOn=1000;
    			blinkOff=1000;
    		}
    	}else{
    		if(blink==1){
    			if(sleepcount>=blinkOn){
        			blink=0;
        			sleepcount=0;
    			}

    		}else{
    			if(sleepcount>=blinkOff){
        			blink=1;
        			sleepcount=0;
    			}
    		}

    		switch(state){
    		case STATE_SYSTEM_START:
        	    gpio_set_level((gpio_num_t)LED_RED, 0);
        	    gpio_set_level((gpio_num_t)LED_GREEN, 0);
        	    gpio_set_level((gpio_num_t)LED_YELLOW, blink);
    			break;
    		case STATE_WAIT_CARD:
        	    gpio_set_level((gpio_num_t)LED_RED, 0);
        	    gpio_set_level((gpio_num_t)LED_GREEN, blink);
        	    gpio_set_level((gpio_num_t)LED_YELLOW, 1);
    			break;
    		case STATE_AUTHORIZING:
        		gpio_set_level((gpio_num_t)LED_RED, blink);
        	    gpio_set_level((gpio_num_t)LED_GREEN, 0);
        	    gpio_set_level((gpio_num_t)LED_YELLOW, 0);
    			break;
    		case STATE_UNLOCKING_DOOR:
        		if(blink){
            	    gpio_set_level((gpio_num_t)LED_RED, 1);
            	    gpio_set_level((gpio_num_t)LED_GREEN, 0);
        		}else{
            	    gpio_set_level((gpio_num_t)LED_RED, 0);
            	    gpio_set_level((gpio_num_t)LED_GREEN, 1);
        		}
        	    gpio_set_level((gpio_num_t)LED_YELLOW, 0);
    			break;
    		case STATE_UNLOCKED_DOOR:
            	gpio_set_level((gpio_num_t)LED_RED, 0);
            	gpio_set_level((gpio_num_t)LED_GREEN, 1);
        	    gpio_set_level((gpio_num_t)LED_YELLOW, 0);
    			break;
    		default:
            	gpio_set_level((gpio_num_t)LED_RED, blink);
            	gpio_set_level((gpio_num_t)LED_GREEN, blink);
        	    gpio_set_level((gpio_num_t)LED_YELLOW, blink);

    		}
    	}

    	lastState = state;

    	sleepcount+=25;
    	vTaskDelay(25 / portTICK_RATE_MS);

    }
}
    
static esp_err_t event_handler(void *ctx, system_event_t *event)
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
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

int check_current() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7,ADC_ATTEN_DB_0);
    int val = adc1_get_raw(ADC1_CHANNEL_7);
    // printf("%d\n", val);
    return val;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    wifi_config_t wifi_config = {};
    strcpy((char*) wifi_config.sta.ssid, (char*) WIFI_SSID);
    strcpy((char*) wifi_config.sta.password, (char*) WIFI_PASS);

    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

bool check_card(char* nfc_id) {
  return authenticate_nfc(nfc_id);
}
    
void app_main()
{
    init();
    red_light.on();
    yellow_light.on();
    green_light.on();
    // client_init();  
    ESP_ERROR_CHECK( nvs_flash_init() );
    initialise_wifi();
    
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                    false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP");
    // authenticate_with_contact_credentials();
    // xTaskCreate(&keepalive_task, "keepalive_task", 8192, NULL, 5, NULL);

    yellow_light.off();
    green_light.off();
    while(1) {
      // printf("%llu\n", esp_timer_get_time());

      // if(check_current() > 50) {
      //   current_detected_time = esp_timer_get_time();
      // }

      if (!power_switch.state() && !state){
        yellow_light.off();
        green_light.off();
        red_light.off();
      }
      if ((!power_switch.state() || (esp_timer_get_time() - current_detected_time > CURRENT_TIMEOUT)) && state) {
        machine_power.off();
        disarm_alarm.off();
        state = 0;
        yellow_light.off();
        green_light.off();
        red_light.off();
        // post_log(CLIENT_TAG "Power+Off","", "","");
      }
      else if(power_switch.state() && !state) {
        red_light.on();
        uint8_t uid[7] = {0};
        uint8_t uid_size = card_reader.poll(uid);
        if (uid_size > 0) {
          char uid_string[15] = {'\0'};
          sprintf(uid_string, "%02x%02x%02x%02x%02x%02x%02x", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6]);
          ESP_LOGI(TAG, "Read card UID: %s", uid_string);
          red_light.off();
          green_light.off();
          yellow_light.on();
          if (check_card(uid_string)) {
            state = 1;
            disarm_alarm.on();
            yellow_light.off();
            green_light.on();
            vTaskDelay(500 / portTICK_PERIOD_MS);
            machine_power.on();
            power_on_time = esp_timer_get_time();
            current_detected_time = esp_timer_get_time();
            // post_log(CLIENT_TAG, userID, nfc_id, "unlock");

            //vTaskDelay(2000 / portTICK_PERIOD_MS);
          }
          else { // show deny
            yellow_light.off();
            red_light.on();
            vTaskDelay(300 / portTICK_PERIOD_MS);
            red_light.off();
            vTaskDelay(200 / portTICK_PERIOD_MS);
            red_light.on();
            vTaskDelay(300 / portTICK_PERIOD_MS);
            red_light.off();
            vTaskDelay(200 / portTICK_PERIOD_MS);
            red_light.on();
            vTaskDelay(300 / portTICK_PERIOD_MS);
            red_light.off();
            vTaskDelay(200 / portTICK_PERIOD_MS);
          }
          esp_task_wdt_reset(); //reset task watchdog
        }

      //   for(int i=0; i< uid_size; i++) {
      //   printf("%d,", uid[i]);
      //   }
      //   printf("\n");
      }
      // printf("%d\n",uxTaskGetStackHighWaterMark(NULL));
    }


    // xTaskCreate(&https_get_task, "https_get_task", 8192, NULL, 5, NULL);
}

