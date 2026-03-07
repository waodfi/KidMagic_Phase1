#ifndef MODULE_BUTTON_H
#define MODULE_BUTTON_H

#include "Config.h"

/// 初始化所有按键的 GPIO。
void btn_init();

/// 每次 loop() 循环调用一次，用于扫描所有按键状态。
void btn_update();

/// 按下按键的那一帧返回 true (下降沿)。
bool btn_justPressed(uint8_t pin);

/// 按键保持按下状态时返回 true。
bool btn_isHeld(uint8_t pin);

/// 松开按键的那一帧返回 true (上升沿)。
bool btn_justReleased(uint8_t pin);

#endif // MODULE_BUTTON_H
