#ifndef MCP_LEDS_H
#define MCP_LEDS_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_system.h>
#include <esp_event_loop.h>
#include <esp_log.h>

extern const char *LED_TAG;

extern "C" {
    #include "ws2812_control.h"
}

#define NUM_LEDS 4

#define M_SAME   0
#define M_SOLID  1
#define M_FLASH  2
#define M_PULSE  3
#define M_CHASE  4
#define M_HEART  5

// #define COLOR    Adafruit_NeoPixel::Color

struct lightState  {
  led_state led_states;
  uint32_t duration;
  uint64_t start_time;
  lightState *next_state;
};

class lightPattern  {
  public:
    lightPattern();
    lightState* first_state;
    lightState* last_state;
    uint32_t duration;
    uint64_t start_time;

    void AddState(lightState);
    void AddState(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
};

class LEDs {
  public:
    LEDs();
    void SetPattern(lightPattern*);
    void lightAll(uint32_t);

    void SetMode(struct lightMode newMode);
    void SetMode(uint8_t m, uint32_t c1, uint32_t c2, int p, int d);
    void Solid(uint32_t c, int d);
    void Flash(uint32_t c1, uint32_t c2, int p, int d);
    void Pulse(uint32_t c, int p, int d);
    void Chase(uint32_t c1, uint32_t c2, int p, int d);
    void Heart(uint32_t c, int p, int d);
    void SetColor1(uint32_t c);
    void SetColor2(uint32_t c);
    void SetPeriod(int period);
    void Update();
    // void led_handler_task(void* arg);

  private:
    QueueHandle_t pattern_queue;
    uint8_t num_LEDs;
    uint8_t pin;
    lightPattern *current_pattern;
    lightState *current_state;
    bool pattern_changed;

    // uint8_t mode;
    // Adafruit_NeoPixel pixels;
    // uint32_t last_change;
    // uint32_t color1;
    // uint32_t color2;
    // uint8_t dim[3];

    // uint16_t period;
    // uint8_t index;
    // uint8_t state;

    // uint8_t tempMode;
    // uint16_t tempDuration;
    // uint32_t tempStarted;
    // uint32_t tempColor1;
    // uint32_t tempColor2;
    // uint8_t tempPeriod;
    // uint8_t tempIndex;
};


// struct lightMode  {
//   uint8_t mode;
//   uint32_t color1;
//   uint32_t color2;
//   uint16_t period;
//   uint16_t duration;
// };



#endif