#ifndef MODULE_LED_H
#define MODULE_LED_H

#include "Config.h"

// --- 单色按键指示灯 -----------------------------------------------
void led_init();
void led_set(uint8_t pin, bool on);
void led_allOff();
void led_allOn();

// --- WS2812B 呼吸灯带 ----------------------------------------------------------
void strip_init();
void strip_setAnimation(LedAnimation anim);
void strip_update();   // 每次 loop() 循环调用

#endif // MODULE_LED_H
