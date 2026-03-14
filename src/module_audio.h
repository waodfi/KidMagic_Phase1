#ifndef MODULE_AUDIO_H
#define MODULE_AUDIO_H

#include "Config.h"

// --- I2S 录音 (INMP441/SPH0645 等数字麦克风) -------------------------------
void audio_init();
bool audio_startRecording();
void audio_stopRecording();
void audio_update();             // 每次 loop() 调用一次
bool audio_isRecording();

// 实时音量 (0-100)
uint8_t audio_getInstantLevel();

// 本次录音统计
uint32_t audio_getSampleCount();
uint8_t audio_getAverageLevel();
uint16_t audio_getPeakSample();

#endif // MODULE_AUDIO_H
