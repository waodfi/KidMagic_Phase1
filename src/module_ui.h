#ifndef MODULE_UI_H
#define MODULE_UI_H

#include "Config.h"

// --- OLED 显示屏 -----------------------------------------------------------
void ui_init();
void ui_showStatus(SystemState state, const char* msg = nullptr);
void ui_showProgress(uint8_t percent);
void ui_showError(const char* msg);

// --- 蜂鸣器 -----------------------------------------------------------------
void buzzer_init();
void buzzer_update();          // 每次 loop() 循环调用
void buzzer_playTone(uint16_t freq, uint16_t durationMs);
void buzzer_beepStart();       // 开机启动和弦
void buzzer_beepShutter();     // 相机快门声
void buzzer_beepSuccess();     // 成功提示音(琶音)
void buzzer_beepError();       // 错误提示音

#endif // MODULE_UI_H
