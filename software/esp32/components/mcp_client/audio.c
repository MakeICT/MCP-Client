#include "audio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "notes.h"


int notes_array[] = {0,33,35,37,39,41,44,46,49,52,55,58,62,65,69,73,78,82,87,93,98,104,110,117,123,131,139,147,156,165,175,185,196,208,220,233,247,262,277,294,311,330,349,370,392,415,440,466,494,523,554,587,622,659,698,740,784,831,880,932,988,1047,1109,1175,1245,1319,1397,1480,1568,1661,1760,1865,1976,2093,2217,2349,2489,2637,2794,2960,3136,3322,3520,3729,3951,4186,4435,4699,4978};


ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
        .freq_hz = 5000,                      // frequency of PWM signal
        .speed_mode = LEDC_HS_MODE,           // timer mode
        .timer_num = LEDC_HS_TIMER,            // timer index
        .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
    };


    ledc_channel_config_t ledc_channel[1] = {
        {
            .channel    = LEDC_HS_CH0_CHANNEL,
            .duty       = 0,
            .gpio_num   = LEDC_HS_CH0_GPIO,
            .speed_mode = LEDC_HS_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_HS_TIMER
        },
    };


void beep(int freq, int duration){

    	ledc_timer.freq_hz = freq,                      // frequency of PWM signal
    	ledc_timer_config(&ledc_timer);

        ledc_set_fade_with_time(ledc_channel[0].speed_mode,ledc_channel[0].channel, LEDC_TEST_DUTY, LEDC_TEST_FADE_TIME);
//		ledc_set_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel, LEDC_TEST_DUTY);
		ledc_update_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel);
        vTaskDelay(duration / portTICK_PERIOD_MS);

		ledc_set_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel, 0);
		ledc_update_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel);
}

void playTune(int ncount,int notes[], int dur[]){
	for(int i = 0; i < ncount; i++){
		beep(notes_array[notes[i]],dur[i]*10);
	}
}


void init_audio(){
    int ch=0;

	ledc_timer_config(&ledc_timer);
	ledc_channel_config(&ledc_channel[ch]);
	ledc_fade_func_install(0);


}


