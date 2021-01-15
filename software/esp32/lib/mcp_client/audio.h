/*
 * audio.h
 *
 *  Created on: Sep 30, 2020
 *      Author: trbloom
 *      pulled from arduino door
 */


#ifndef AUDIO_H
#define AUDIO_H

#include "notes.h"

//#include <NewTone.h>

//struct tune  {
//  uint8_t length;
//  uint8_t notes[30];
//  uint8_t durations[30];
//};
//
//class Audio
//{
//  public:
//    Audio(byte);
//    void SetDebugPort(SoftwareSerial* dbgPort);
//    void Play(struct tune newTune);
//    void Play(byte melody[], byte durations[], byte length);
//    void Update();
//
//  private:
//    SoftwareSerial* debugPort;
//    byte audioPin;
//    boolean playing;
//    uint32_t currentNoteStartTime;
//    uint8_t noteIndex;
//    struct tune currentTune;
//};


#ifndef AUDIO_PIN
#define AUDIO_PIN 16
#endif


#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO       AUDIO_PIN
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0

#define LEDC_TEST_DUTY         (4000)
#define LEDC_TEST_FADE_TIME    (200)


void playTune(int ncount,int notes[], int dur[]);
void init_audio();

#endif
