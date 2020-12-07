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



/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/

/* FreeRTOS event group to signal when we are connected*/
//static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


static int s_retry_num = 0;

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define WIFI_SSID "mywifissid"
*/
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASSWORD
#define ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#define SERVER CONFIG_SERVER
#define SERVER2 CONFIG_SERVER2
#define PORT CONFIG_PORT

#define ESP_INTR_FLAG_DEFAULT 0

//#define CONFIG_DOOR_DELAY 500
//#define CONFIG_DOOR_OPEN 1500
#define CONFIG_ALARM_ARM_DELAY 2000
#define CONFIG_ALARM_ARM_RETRY 4
#define CONFIG_ALARM_DISARM_DELAY 5000


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
/**
 * @brief gpio_isr_handler  Handle button press interrupt
 */

bool alarm_active = 0;
int  arm_state_needed = 0;
bool motion    = 0;
int  arm_cycle_count = 0;
TickType_t motion_timeout = 0;

static xQueueHandle gpio_evt_queue = NULL;

/* Letsencrypt root cert, taken from server_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
// extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
// extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

static void IRAM_ATTR gpio_isr_handler(void* arg)
{

    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}
bool pvalue=0;
bool arm_button_last=0;
bool bell_button_last=0;
static void gpio_task_io_handler(void* arg)
{
    uint32_t io_num=99999;
    for(;;) {
    	vTaskDelay(100 / portTICK_RATE_MS);
    	if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
    		pvalue=gpio_get_level((gpio_num_t)io_num);

//            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level((gpio_num_t)io_num));
            if(io_num == ALARM_STATE_INPUT){
            	if(pvalue==0 && alarm_active ==0){
            		printf("Alarm on\n");
            		alarm_active = 1;
            	}if(pvalue==1 && alarm_active ==1){
            		printf("Alarm off\n");
            		alarm_active = 0;
//            	}else{
//            		printf("no change skipping\n");
            	}

            }else if(io_num == DOOR_BELL_INPUT){
            	if(arm_button_last!=pvalue && pvalue==0){
            		printf("Ding Dong!\n");
            	}
            	arm_button_last=pvalue;
            }else if(io_num == ALARM_ARM_INPUT){

            	if(arm_button_last!=pvalue){
                	if(arm_cycle_count>1){
                		arm_cycle_count = 0;//resetting arm count
                	}

                	if(arm_state_needed<=0){
                    	ESP_LOGI(TAG, "Alarm button pushed.");
            			if(xTaskGetTickCount()<motion_timeout){
    						ESP_LOGI(TAG, "Motion detected can't arm. %d < %d", xTaskGetTickCount(), motion_timeout);
    					}else{
    						arm_state_needed++;
    					}

                	}else{
                		ESP_LOGI(TAG, "Alarm button pushed.   Skipping");
                	}
//            	}else{
//            		ESP_LOGI(TAG, "AButton no change skipping");
            	}
            	arm_button_last=pvalue;
//            }else if(io_num == ALARM_MOTION_INPUT){
//            	if(pvalue==0){
//            		printf("Alarm motion\n");
//            		ESP_LOGI(TAG, "Arm motion active. Resetting countdown");
//            		motion_timeout = xTaskGetTickCount()+(30*1000 / portTICK_PERIOD_MS);
//            	}
//
            }else{
            	//UNKNOWN IO
                printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level((gpio_num_t)io_num));
            }


        }
    }
}

//typedef struct {
//  int gpioNum;
//  int ledMode;
//  void * _stateVars;
//} ledState_t;
//
//ledState_t LED_MODES[] = {
//  {.gpioNum = 0, .gpioNum = LED_OUTPUT, .ledType = LED_WS2812B_V2, .brightLimit = 24, .numPixels =  1},
//};

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

		if (s_retry_num < ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
	        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
		} else {
			//			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
			ESP_LOGI(TAG, "failed to connect, sleeping 5 minutes");
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


void init(void)
{

    ESP_LOGI(TAG,"Setting up pins");

    gpio_config_t io_conf;
    gpio_config_t io_conf2;
    gpio_config_t io_conf3;
    gpio_config_t io_conf4;


    io_conf.intr_type = (gpio_int_type_t)GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = (gpio_pulldown_t)1;
    io_conf.pull_up_en = (gpio_pullup_t)0;
    gpio_config(&io_conf);

    io_conf2.intr_type = (gpio_int_type_t)GPIO_INTR_ANYEDGE;
    io_conf2.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf2.mode = GPIO_MODE_INPUT;
    io_conf2.pull_down_en = (gpio_pulldown_t) 0;
    io_conf2.pull_up_en = (gpio_pullup_t)1;
    gpio_config(&io_conf2);



    io_conf3.intr_type = (gpio_int_type_t) GPIO_PIN_INTR_DISABLE;
    io_conf3.mode = GPIO_MODE_OUTPUT;
    io_conf3.pin_bit_mask = GPIO_OUTPUT_LED_PIN_SEL;
    io_conf3.pull_down_en = (gpio_pulldown_t)1;
    io_conf3.pull_up_en = (gpio_pullup_t)0;
    gpio_config(&io_conf3);



    io_conf4.intr_type = (gpio_int_type_t)GPIO_INTR_ANYEDGE;
    io_conf4.pin_bit_mask = GPIO_INPUT2_PIN_SEL;
    io_conf4.mode = GPIO_MODE_INPUT;
    io_conf4.pull_down_en = (gpio_pulldown_t) 1;
    io_conf4.pull_up_en = (gpio_pullup_t)0;
    gpio_config(&io_conf4);



    //change gpio intrrupt type for one pin
//    gpio_set_intr_type(ALARM_STATE_INPUT, (gpio_int_type_t) GPIO_INTR_ANYEDGE);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task_io_handler, "gpio_task_io", 2048, NULL, 10, NULL);
    xTaskCreate(led_handler_task, "led_handler_task", 2048, NULL, 10, NULL);


    //    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add((gpio_num_t)ALARM_STATE_INPUT, gpio_isr_handler, (void*) ALARM_STATE_INPUT);
    gpio_isr_handler_add((gpio_num_t)ALARM_ARM_INPUT, gpio_isr_handler, (void*) ALARM_ARM_INPUT);
    gpio_isr_handler_add((gpio_num_t)DOOR_BELL_INPUT, gpio_isr_handler, (void*) DOOR_BELL_INPUT);
    //    gpio_isr_handler_add((gpio_num_t)ALARM_MOTION_INPUT, gpio_isr_handler, (void*) ALARM_MOTION_INPUT);

    gpio_set_level((gpio_num_t)DOOR_STRIKE_RELAY, 0);
    gpio_set_level((gpio_num_t)ALARM_DISARM_RELAY, 0);
    gpio_set_level((gpio_num_t)ALARM_ARM_RELAY, 0);

    gpio_set_level((gpio_num_t)LED_RED, 0);
    gpio_set_level((gpio_num_t)LED_YELLOW, 0);
    gpio_set_level((gpio_num_t)LED_GREEN, 0);


//    gpio_pad_select_gpio((gpio_num_t)ALARM_STATE_INPUT);
//    gpio_set_direction((gpio_num_t)ALARM_STATE_INPUT, GPIO_MODE_INPUT);
//    ESP_LOGI(TAG,"debug p ");
//
//    gpio_pad_select_gpio((gpio_num_t)ALARM_MOTION_INPUT);
//    gpio_set_direction((gpio_num_t)ALARM_MOTION_INPUT, GPIO_MODE_INPUT);
//    ESP_LOGI(TAG,"debug p ");
//    gpio_pad_select_gpio((gpio_num_t)ALARM_ARM_INPUT);
//    gpio_set_direction((gpio_num_t)ALARM_ARM_INPUT, GPIO_MODE_INPUT);
//    gpio_set_intr_type((gpio_num_t)ALARM_ARM_INPUT, GPIO_INTR_NEGEDGE); //release of button
//    ESP_LOGI(TAG,"debug p ");
//
//    gpio_pad_select_gpio((gpio_num_t)ALARM_ARM_RELAY);
//    gpio_set_direction((gpio_num_t)ALARM_ARM_RELAY, GPIO_MODE_OUTPUT);
//    ESP_LOGI(TAG,"debug p ");
//    gpio_pad_select_gpio((gpio_num_t)ALARM_DISARM_RELAY);
//    gpio_set_direction((gpio_num_t)ALARM_DISARM_RELAY , GPIO_MODE_OUTPUT);
//    ESP_LOGI(TAG,"debug p ");
//    gpio_pad_select_gpio((gpio_num_t)DOOR_STRIKE_RELAY);
//    gpio_set_direction((gpio_num_t)DOOR_STRIKE_RELAY, GPIO_MODE_OUTPUT);
}
    
bool check_card(char* nfc_id) {
  return authenticate_nfc(nfc_id);
}
    
void app_main()
{

    init();

    struct rb_tree *tree = NULL; //loadCache();

    bool unlockdoor=0;


    ESP_LOGI(TAG, "Starting loop");

    while(1) {
    	state = STATE_WAIT_CARD;

//        ESP_LOGI(TAG, "looping,%d %d %d",gpio_get_level((gpio_num_t)ALARM_STATE_INPUT),gpio_get_level((gpio_num_t)ALARM_ARM_INPUT),gpio_get_level((gpio_num_t)ALARM_MOTION_INPUT));
//    	vTaskDelay(100 / portTICK_PERIOD_MS);

    	// printf("%llu\n", esp_timer_get_time());
        uint8_t uid[7] = {0};

    	unlockdoor=0;

    	vTaskDelay(25 / portTICK_PERIOD_MS);

        uint8_t uid_size = card_reader.poll(uid);

        char uid_string[15] = {'\0'};

        if (uid_size > 0) {
          state = STATE_AUTHORIZING;
          sprintf(uid_string, "%02x%02x%02x%02x%02x%02x%02x", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6]);
          ESP_LOGI(TAG, "Read card UID: %s", uid_string);
          if( xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT ){
			  ESP_LOGI(TAG, "Connected to AP");
			  // authenticate_with_contact_credentials();
			  // xTaskCreate(&keepalive_task, "keepalive_task", 8192, NULL, 5, NULL);

	          if (check_card(uid_string)) {
	            state = STATE_UNLOCKING_DOOR;
//	            vTaskDelay(500 / portTICK_PERIOD_MS);
//	            power_on_time = esp_timer_get_time();
//	            current_detected_time = esp_timer_get_time();
	            ESP_LOGI(TAG, "Card Authorized");
	            unlockdoor=1;
	          }else { // show deny
	        	  state = STATE_CARD_REJECT;
	              ESP_LOGI(TAG, "Card Unuthorized");
	              unlockdoor=0;
	          }
          }else{
			  ESP_LOGI(TAG, "Wifi AP connection down.");


          }

          if (tree) {
//              ESP_LOGI(TAG, "debug ");
			  struct BADGEINFO *sbadge = ( struct BADGEINFO * ) malloc(sizeof( struct BADGEINFO));
//              ESP_LOGI(TAG, "debug ");
			  sbadge->uid_string = (char*)malloc(sizeof(uid_string));
//              ESP_LOGI(TAG, "debug ");
			  strcpy(sbadge->uid_string,uid_string);
//              ESP_LOGI(TAG, "debug ");

			  struct BADGEINFO *badgerecord = ( struct BADGEINFO * ) rb_tree_find(tree, sbadge);

//              ESP_LOGI(TAG, "debug 1");

			  if (badgerecord!=NULL) {
//				  	  ESP_LOGI(TAG, "debug 4");
					  ESP_LOGI(TAG, "found badge %s  (%d)\n", badgerecord->uid_string , badgerecord->active);

					  free(sbadge->uid_string);
					  free(sbadge);

			  } else {
//	              ESP_LOGI(TAG, "debug 5");
				  if(unlockdoor ){
//		              ESP_LOGI(TAG, "debug 10");
					  ESP_LOGI(TAG, "new active badge %s  (%d) adding to cache\n", sbadge->uid_string , sbadge->active);
					  rb_tree_insert(tree, sbadge);
					  badgerecord=sbadge;
					  badgerecord->needswrite = 1;
				  }else{
//		              ESP_LOGI(TAG, "debug 2");
					  free(sbadge->uid_string);
					  free(sbadge);
				  }
			  }

			  if(unlockdoor ){
//	              ESP_LOGI(TAG, "debug 9");
				  if(badgerecord && badgerecord->active){
//		              ESP_LOGI(TAG, "debug 7");
					  badgerecord->scancount++;
//		              ESP_LOGI(TAG, "debug 8");
					  badgerecord->needswrite = 1;
				  }
			  }else{
//	              ESP_LOGI(TAG, "debug 6");
					if(badgerecord!=NULL && badgerecord->active==1){
//						ESP_LOGI(TAG, "debug 11");
						unlockdoor=1;
					}
			  }
          }


//          ESP_LOGI(TAG, "debug 15");
          //bool needsalarmcode=0;
          if(unlockdoor){
        	  state = STATE_UNLOCKING_DOOR;
        	  arm_state_needed=-1;
        	  arm_cycle_count=0;

        	  gpio_set_level((gpio_num_t)ALARM_DISARM_RELAY, 1);
              vTaskDelay( CONFIG_DOOR_DELAY / portTICK_PERIOD_MS);
        	  gpio_set_level((gpio_num_t)ALARM_DISARM_RELAY, 0);

        	  ESP_LOGI(TAG, "Checking alarm state.");
        	  int retryCount=1;

        	  while(alarm_active==1 && arm_state_needed<1 && retryCount<=10){ //5 second wait to disarm.
        		  if(retryCount%2==0){
                	  gpio_set_level((gpio_num_t)ALARM_DISARM_RELAY, 1);
                      vTaskDelay( 500 / portTICK_PERIOD_MS);
                	  gpio_set_level((gpio_num_t)ALARM_DISARM_RELAY, 0);
        		  }
        		  ESP_LOGI(TAG, "Waiting for disarm");
        		  vTaskDelay(500 / portTICK_PERIOD_MS);
        		  //FIXME: add timeout waiting for door.
        		  retryCount++;
        	  }

        	  if(alarm_active){
        		  ESP_LOGI(TAG, "Failed to disarm Alarm.");
        		  state = STATE_UNLOCKED_DOOR;
        	  }else{
            	  if(arm_state_needed>0){
            		  ESP_LOGI(TAG, "Arming alarm cancel unlock");
            	  }else{
            		  state = STATE_UNLOCKED_DOOR;
            		  ESP_LOGI(TAG, "Alarm off");
                	  ESP_LOGI(TAG, "Unlocking door");
                	  gpio_set_level((gpio_num_t)DOOR_STRIKE_RELAY, 1);
                      vTaskDelay(CONFIG_DOOR_OPEN / portTICK_PERIOD_MS);
                	  gpio_set_level((gpio_num_t)DOOR_STRIKE_RELAY, 0);
                	  ESP_LOGI(TAG, "Locking door");
            	  }
        	  }
          }

          esp_task_wdt_reset(); //reset task watchdog
        }

        gpio_set_level((gpio_num_t)DOOR_STRIKE_RELAY, 0);

   }
}

