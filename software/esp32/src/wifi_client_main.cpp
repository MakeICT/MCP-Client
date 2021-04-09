/*H****************************************************************************
* FILENAME : wifi_client_main.cpp
 *
* DESCRIPTION :
*       Main loop for the MCP client. Sets everything up and launches 
        necessary freeRTOS tasks.
 *
* NOTES :
 *
* AUTHORS : Christian Kindel, Tom Bloom
 *
*H*/

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
#include <esp_timer.h>
#include <driver/gpio.h>

// #include "utils.h"
#include "definitions.h"
#include "mcp_api.h"
#include "mcp_network.h"
#include "mcp_leds.h"
#include "reader.h"
#include "sdkconfig.h"

extern "C"
{
#include "../mcp_client/rb_tree.h"
#include "../mcp_client/audio.h"
#include "../mcp_client/ws2812_control.h"
}

static const char *TAG = "wifi_client_main";

int notes_scale[] = {48, 50, 52, 53, 55, 57, 59, 60};
int dur_scale[] = {21, 21, 21, 21, 21, 21, 21, 12};

int notes_fanfare[] = {34, 34, 32, 34, 38, 40, 38, 40, 43};
int dur_fanfare[] = {21, 07, 07, 07, 21, 07, 07, 07, 42};

int notes_hb[] = {54, 54, 56, 54, 59, 58, 00, 54, 54, 56, 54, 61, 59, 00, 54, 54, 66, 63, 59, 58, 56, 00, 64, 64, 63, 59, 61, 59};
int dur_hb[] = {14, 06, 20, 20, 20, 20, 20, 14, 06, 20, 20, 20, 20, 20, 14, 06, 20, 20, 20, 20, 40, 10, 14, 06, 20, 20, 23, 17};

struct BADGEINFO
{
    char *uid_string;
    bool active = 0;
    int scancount = 0;
    bool needswrite = 0;
};

extern int badge_compare_callback(struct rb_tree *self, struct rb_node *node_a, struct rb_node *node_b)
{
    BADGEINFO *a = (BADGEINFO *)node_a->value;
    BADGEINFO *b = (BADGEINFO *)node_b->value;
    return strcmp(a->uid_string, b->uid_string);
}

enum state
{
    STATE_UNKNOWN,
    STATE_SYSTEM_START,
    STATE_NO_NETWORK,
    STATE_ALARM_ARMED,
    STATE_WAIT_CARD,
    STATE_AUTHORIZING,
    STATE_CARD_REJECT,
    STATE_UNLOCKING_DOOR,
    STATE_UNLOCKED_DOOR
};

extern "C"
{
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

bool alarm_active = 0;
int arm_state_needed = 0;
bool motion = 0;
TickType_t motion_timeout = 0;

Network network = Network();
LEDs lights = LEDs();

/* Letsencrypt root cert, taken from server_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
// extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
// extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

// static const char *REQUEST = "POST " AUTH_ENDPOINT "?email=" CONFIG_USERNAME    "&password=" CONFIG_PASSWORD "\r\n"
//     "Host: " WEB_SERVER "\r\n"
//     "Content-Type: application/x-www-form-urlencoded\r\n"
//     "\r\n";

static xQueueHandle gpio_evt_queue = NULL;

/**
 * @brief gpio_isr_handler  Handle button press interrupt
 */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_io_handler(void *arg)
{
    static bool pvalue = 0;
    static bool arm_button_last = 0;
    static bool bell_button_last = 0;
    uint32_t io_num = 99999;

    for (;;)
    {
        // vTaskDelay(100 / portTICK_RATE_MS);
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            pvalue = gpio_get_level((gpio_num_t)io_num);

            printf("GPIO[%d] intr, val: %d\n", io_num, pvalue);
            if (io_num == ALARM_STATE_INPUT)
            {
                if (pvalue == 1 && alarm_active == 1)
                {
                    printf("Alarm off\n");
                    alarm_active = 0;
                }
                if (pvalue == 0 && alarm_active == 0)
                {
                    printf("Alarm on\n");
                    alarm_active = 1;
                }
            }
            else if (io_num == DOOR_BELL_INPUT)
            {
                if (arm_button_last != pvalue && pvalue == 0)
                {
                    printf("Ding Dong!\n");
                }
                arm_button_last = pvalue;
            }
            else if (io_num == ALARM_ARM_INPUT)
            {

                if (arm_button_last != pvalue)
                {
                    if (arm_state_needed <= 0)
                    {
                        ESP_LOGI(TAG, "Alarm button pushed.");
                        if (xTaskGetTickCount() < motion_timeout)
                        {
                            ESP_LOGI(TAG, "Motion detected can't arm. %d < %d", xTaskGetTickCount(), motion_timeout);
                        }
                        else
                        {
                            arm_state_needed++;
                        }
                    }
                    else
                    {
                        ESP_LOGI(TAG, "Alarm button pushed.   Skipping");
                    }
                }
                arm_button_last = pvalue;
                // }else if(io_num == ALARM_MOTION_INPUT){
                // if(pvalue==0){
                //     printf("Alarm motion\n");
                //     ESP_LOGI(TAG, "Arm motion active. Resetting countdown");
                //     motion_timeout = xTaskGetTickCount()+(30*1000 / portTICK_PERIOD_MS);
                // }
    
            }
            else
            {
                //UNKNOWN IO
                printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level((gpio_num_t)io_num));
            }
        }
    }
}

static void led_handler_task(void *arg)
{
    while (1)
    {
        lights.Update();

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

lightPattern *system_start_pattern = new lightPattern();
lightPattern *authorizing_pattern = new lightPattern();
lightPattern *card_reject_pattern = new lightPattern();
lightPattern *unlocking_door_pattern = new lightPattern();
lightPattern *unlocked_door_pattern = new lightPattern();
lightPattern *wait_card_pattern = new lightPattern();

static void setup_light_patterns()
{
    system_start_pattern->AddState(25, LED_YELLOW, LED_RED, LED_YELLOW, LED_YELLOW);
    system_start_pattern->AddState(150, LED_OFF, LED_OFF, LED_OFF, LED_RED);

    authorizing_pattern->AddState(100, LED_OFF, LED_YELLOW, LED_OFF, LED_YELLOW);
    authorizing_pattern->AddState(100, LED_OFF, LED_OFF, LED_OFF, LED_OFF);

    card_reject_pattern->AddState(100, LED_OFF, LED_RED, LED_RED, LED_RED);
    card_reject_pattern->AddState(100, LED_OFF, LED_OFF, LED_OFF, LED_OFF);

    unlocking_door_pattern->AddState(250, LED_GREEN, LED_YELLOW, LED_GREEN, LED_YELLOW);
    unlocking_door_pattern->AddState(250, LED_YELLOW, LED_GREEN, LED_YELLOW, LED_GREEN);

    unlocked_door_pattern->AddState(1000, LED_GREEN, LED_GREEN, LED_GREEN, LED_GREEN);

    wait_card_pattern->AddState(1000, LED_RED, LED_RED, LED_OFF, LED_OFF);
    wait_card_pattern->AddState(100, LED_OFF, LED_OFF, LED_OFF, LED_OFF);
}

struct rb_tree *loadCache()
{
    ESP_LOGI(TAG, "Allocating cache true");

    // rb_tree_node_cmp_f cmp = badge_compare_callback;
    struct rb_tree *tree = rb_tree_create(badge_compare_callback);
    tree = rb_tree_create(badge_compare_callback);

    // bool download_success = load_nfc_list();

    if (tree)
    {
        // Use the tree here...
        // for (int i = 0; i < CACHE_SIZE-1; i++) {
        //     struct BADGEINFO *badge = ( struct BADGEINFO * ) malloc(sizeof( struct BADGEINFO));
        //     badge->uid_string = (char*)malloc(sizeof("04b329d2784c81"));
        //     strcpy(badge->uid_string,"04b329d2784c81");
        //     badge->active = 1;
        //     badge->scancount = 0;
        //     if(i%100==1){
        //     ESP_LOGI(TAG, "%d",i);
        //     }

        //     // Default insert, which allocates internal rb_nodes for you.
        //     rb_tree_insert(tree, badge);
        // }

        struct BADGEINFO *badge = (struct BADGEINFO *)malloc(sizeof(struct BADGEINFO));
        badge->uid_string = (char *)malloc(sizeof(""));
        strcpy(badge->uid_string, "NOBADGE");
        badge->active = 0;
        badge->scancount = 0;
        rb_tree_insert(tree, badge);

        // struct BADGEINFO *sbadge = ( struct BADGEINFO * ) malloc(sizeof( struct BADGEINFO));
        // sbadge->uid_string = (char*)malloc(sizeof("04b329d2784c80"));
        // strcpy(sbadge->uid_string,"04b329d2784c80");
        // sbadge->active = 0;
        // sbadge->scancount = 0;


        // struct BADGEINFO *f = ( struct BADGEINFO * ) rb_tree_find(tree, sbadge);
        // if (f) {
        //         fprintf(stdout, "found badge %s  (%d))\n", f->uid_string,f->active);
        // } else {
        //     printf("not found\n");
        // }

        // free(sbadge->uid_string);
        // free(sbadge);

        // Dealloc call can take optional parameter to notify on each node
        // being deleted so you can free the node and/or your object:
        // rb_tree_dealloc(tree, NULL);
    }

    ESP_LOGI(TAG, "Finished allocating cache");
    return tree;
}

void init(void)
{
    ESP_LOGI(TAG, "Setting up pins");

    init_audio();
    setup_light_patterns();
    xTaskCreate(led_handler_task, "led_handler_task", 2048, NULL, 10, NULL);
    lights.SetPattern(system_start_pattern);

    gpio_config_t io_conf;
    gpio_config_t io_conf2;
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
    io_conf2.pull_down_en = (gpio_pulldown_t)0;
    io_conf2.pull_up_en = (gpio_pullup_t)0;
    gpio_config(&io_conf2);

    io_conf4.intr_type = (gpio_int_type_t)GPIO_INTR_ANYEDGE;
    io_conf4.pin_bit_mask = GPIO_INPUT2_PIN_SEL;
    io_conf4.mode = GPIO_MODE_INPUT;
    io_conf4.pull_down_en = (gpio_pulldown_t)1;
    io_conf4.pull_up_en = (gpio_pullup_t)0;
    gpio_config(&io_conf4);

    // change gpio intrrupt type for one pin
    // gpio_set_intr_type(ALARM_STATE_INPUT, (gpio_int_type_t) GPIO_INTR_ANYEDGE);

    // create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    // start gpio task
    xTaskCreate(gpio_task_io_handler, "gpio_task_io", 2048, NULL, 10, NULL);

    /* Install ISR routine */
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add((gpio_num_t)ALARM_STATE_INPUT, gpio_isr_handler, (void *)ALARM_STATE_INPUT);
    gpio_isr_handler_add((gpio_num_t)ALARM_ARM_INPUT, gpio_isr_handler, (void *)ALARM_ARM_INPUT);
    gpio_isr_handler_add((gpio_num_t)DOOR_BELL_INPUT, gpio_isr_handler, (void *)DOOR_BELL_INPUT);
    //    gpio_isr_handler_add((gpio_num_t)ALARM_MOTION_INPUT, gpio_isr_handler, (void*) ALARM_MOTION_INPUT);

    gpio_set_level((gpio_num_t)DOOR_STRIKE_RELAY, 0);
    gpio_set_level((gpio_num_t)ALARM_DISARM_RELAY, 0);
    gpio_set_level((gpio_num_t)ALARM_ARM_RELAY, 0);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    network.setup(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
    // network.setup();
    network.init();
    network.start();

    xTaskCreate(&http_api_task, "http_api_task", 8192, NULL, 5, NULL);

    // disable wifi power saving to prevent GPIO 36 and 39 from constantly creating interrupts
    // https://github.com/espressif/esp-idf/issues/1096
    // https://github.com/espressif/esp-idf/issues/4585
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    card_reader.init();
}

void app_main()
{
    init();

    struct rb_tree *tree = NULL;
    //loadCache();

    ESP_LOGI(TAG, "Starting loop");
    while (1)
    {
        static bool unlockdoor = 0;

        if (state != STATE_WAIT_CARD)
        {
            state = STATE_WAIT_CARD;
            lights.SetPattern(wait_card_pattern);
        }

        // ESP_LOGI(TAG, "looping,%d %d %d",gpio_get_level((gpio_num_t)ALARM_STATE_INPUT),gpio_get_level((gpio_num_t)ALARM_ARM_INPUT),gpio_get_level((gpio_num_t)ALARM_MOTION_INPUT));
        // vTaskDelay(100 / portTICK_PERIOD_MS);

        // printf("%llu\n", esp_timer_get_time());

        vTaskDelay(25 / portTICK_PERIOD_MS);

        uint8_t uid[7] = {0};
        uint8_t uid_size = card_reader.poll(uid);

        if (uid_size > 0)
        {
            state = STATE_AUTHORIZING;
            lights.SetPattern(authorizing_pattern);

            char uid_string[15] = {'\0'};
            sprintf(uid_string, "%02x%02x%02x%02x%02x%02x%02x", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6]);
            ESP_LOGI(TAG, "Read card UID: %s", uid_string);
            if (network.isConnected())
            {
                ESP_LOGI(TAG, "Network is connected");

                if (authenticate_nfc(uid_string) > 0)
                {
                    state = STATE_UNLOCKING_DOOR;
                    lights.SetPattern(unlocking_door_pattern);
                    // vTaskDelay(500 / portTICK_PERIOD_MS);
                    // power_on_time = esp_timer_get_time();
                    // current_detected_time = esp_timer_get_time();
                    ESP_LOGI(TAG, "Card Authorized");

                    // playTune(8,notes_scale,dur_scale);

                    unlockdoor = 1;
                }
                else
                { // show deny
                    state = STATE_CARD_REJECT;
                    lights.SetPattern(card_reject_pattern);
                    ESP_LOGI(TAG, "Card Unauthorized");
                    unlockdoor = 0;
                    vTaskDelay(3000 / portTICK_PERIOD_MS);
                }
            }
            else
            {
                ESP_LOGW(TAG, "Network connection down.");
                network.restart();
            }

            if (tree)
            {
                struct BADGEINFO *sbadge = (struct BADGEINFO *)malloc(sizeof(struct BADGEINFO));
                sbadge->uid_string = (char *)malloc(sizeof(uid_string));
                strcpy(sbadge->uid_string, uid_string);

                struct BADGEINFO *badgerecord = (struct BADGEINFO *)rb_tree_find(tree, sbadge);

                if (badgerecord != NULL)
                {
                    ESP_LOGI(TAG, "found badge %s  (%d)\n", badgerecord->uid_string, badgerecord->active);

                    free(sbadge->uid_string);
                    free(sbadge);
                }
                else
                {
                    if (unlockdoor)
                    {
                        ESP_LOGI(TAG, "new active badge %s  (%d) adding to cache\n", sbadge->uid_string, sbadge->active);
                        rb_tree_insert(tree, sbadge);
                        badgerecord = sbadge;
                        badgerecord->needswrite = 1;
                    }
                    else
                    {
                        free(sbadge->uid_string);
                        free(sbadge);
                    }
                }

                if (unlockdoor)
                {
                    if (badgerecord && badgerecord->active)
                    {
                        badgerecord->scancount++;
                        badgerecord->needswrite = 1;
                    }
                }
                else
                {
                    if (badgerecord != NULL && badgerecord->active == 1)
                    {
                        unlockdoor = 1;
                    }
                }
            }

            if (unlockdoor)
            {
                int pvalue = gpio_get_level((gpio_num_t)ALARM_STATE_INPUT);
                if (pvalue == 0 && alarm_active == 0)
                {
                    printf("Alarm on\n");
                    alarm_active = 1;
                }

                state = STATE_UNLOCKING_DOOR;
                lights.SetPattern(unlocking_door_pattern);
                arm_state_needed = -1;

                ESP_LOGI(TAG, "Checking alarm state.");
                int retryCount = 1;

                while (alarm_active == 1 && arm_state_needed < 1 && retryCount <= 10)
                { //5 second wait to disarm.
                    ESP_LOGI(TAG, "Waiting for disarm");
                    if (retryCount % 2 == 0)
                    {
                        gpio_set_level((gpio_num_t)ALARM_DISARM_RELAY, 1);
                        vTaskDelay(500 / portTICK_PERIOD_MS);
                        gpio_set_level((gpio_num_t)ALARM_DISARM_RELAY, 0);
                    }
                    vTaskDelay(CONFIG_DOOR_DELAY / portTICK_PERIOD_MS);
                    //FIXME: add timeout waiting for door.
                    retryCount++;
                }

                if (alarm_active)
                {
                    ESP_LOGI(TAG, "Failed to disarm Alarm.");
                }
                else
                {
                    if (arm_state_needed > 0)
                    {
                        ESP_LOGI(TAG, "Arming alarm cancel unlock");
                    }
                    else
                    {
                        state = STATE_UNLOCKED_DOOR;
                        lights.SetPattern(unlocked_door_pattern);
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
