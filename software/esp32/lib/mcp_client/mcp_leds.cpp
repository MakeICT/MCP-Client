#include "mcp_leds.h"

const char *LED_TAG = "MCP_LEDS";

lightPattern::lightPattern()
{
    printf("initializing lightPattern\n");
    first_state = NULL;
    last_state = NULL;
}

void lightPattern::AddState(uint32_t duration, uint32_t led_state_0, 
        uint32_t led_state_1, uint32_t led_state_2 ,uint32_t led_state_3)
{
    lightState state = {{led_state_0, led_state_1, led_state_2, led_state_3}, duration, 0, NULL};
    AddState(state);
}

void lightPattern::AddState(lightState state)
{
    lightState *new_state = (lightState*) malloc(sizeof(lightState));
    if(first_state == NULL) {
        first_state = new_state;
        last_state = new_state;
    }
    else {
        last_state->next_state = new_state;
    }
    new_state->led_states.leds[0] = state.led_states.leds[0];
    new_state->led_states.leds[1] = state.led_states.leds[1];
    new_state->led_states.leds[2] = state.led_states.leds[2];
    new_state->led_states.leds[3] = state.led_states.leds[3];
    new_state->duration = state.duration * 1000;
    new_state->next_state = first_state;
}

LEDs::LEDs() {
    pattern_queue = xQueueCreate(10, sizeof(lightPattern*));
    ws2812_control_init();
    current_pattern = NULL;
    // xTaskCreate(led_handler_task, "led_handler_task", 2048, NULL, 10, NULL);
}

void LEDs::SetPattern(lightPattern* new_pattern)
{
    current_pattern = new_pattern;
    current_state = new_pattern->first_state;
    new_pattern->first_state->start_time = esp_timer_get_time();
}

void LEDs::Update()
{ 
    // ESP_LOGI(LED_TAG, "LED update");
    if(current_pattern != NULL) {
        // ESP_LOGI(LED_TAG, "Pattern not NULL");
        uint32_t elapsed_time = esp_timer_get_time() - current_state->start_time;
        ESP_LOGI(LED_TAG, "%d", elapsed_time);    
        if((esp_timer_get_time() - current_state->start_time) > current_state->duration) {
            ESP_LOGI(LED_TAG, "Changing state");
            current_state = current_state->next_state;
            current_state->start_time = esp_timer_get_time();
        }
        ws2812_write_leds(current_state->led_states);
    }
}
