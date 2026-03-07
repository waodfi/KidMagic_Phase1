#include "module_led.h"
#include <FastLED.h>

// ============================================================================
//  单色按键指示灯
// ============================================================================
static const uint8_t ledPins[] = {
    LED_START, LED_PHOTO, LED_RECORD, LED_GENERATE, LED_PLAY
};
static const uint8_t LED_COUNT = sizeof(ledPins) / sizeof(ledPins[0]);

void led_init() {
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        pinMode(ledPins[i], OUTPUT);
        digitalWrite(ledPins[i], LOW); // LOW = 熄灭 (0V 电源)
    }
}

void led_set(uint8_t pin, bool on) {
    digitalWrite(pin, on ? HIGH : LOW); // HIGH = 点亮 (提供电流)
}

void led_allOff() {
    for (uint8_t i = 0; i < LED_COUNT; i++)
        digitalWrite(ledPins[i], LOW);
}

void led_allOn() {
    for (uint8_t i = 0; i < LED_COUNT; i++)
        digitalWrite(ledPins[i], HIGH);
}

// ============================================================================
//  WS2812B 非阻塞动画引擎
// ============================================================================
static CRGB leds[WS2812_NUM];
static LedAnimation currentAnim = ANIM_NONE;
static uint32_t animTimer       = 0;
static uint16_t animStep        = 0;

void strip_init() {
    FastLED.addLeds<WS2812B, WS2812_PIN, GRB>(leds, WS2812_NUM);
    FastLED.setBrightness(WS2812_MAX_BRIGHTNESS);
    fill_solid(leds, WS2812_NUM, CRGB::Black);
    FastLED.show();
}

void strip_setAnimation(LedAnimation anim) {
    if (anim == currentAnim) return;
    currentAnim = anim;
    animStep    = 0;
    animTimer   = millis();
    fill_solid(leds, WS2812_NUM, CRGB::Black);
    FastLED.show();
}

// ---------------------------------------------------------------------------
//  动画辅助函数 — 全由 millis() 驱动，零 delay()
// ---------------------------------------------------------------------------

// 平滑正弦波呼吸灯 (青色)
static void anim_idleBreathe() {
    uint32_t now = millis();
    if (now - animTimer < 30) return;  // 约 33 fps
    animTimer = now;
    uint8_t brightness = beatsin8(12, 10, WS2812_MAX_BRIGHTNESS);  // 12 BPM (每分钟12次)
    fill_solid(leds, WS2812_NUM, CRGB(0, brightness, brightness));
    FastLED.show();
}

// 快速白光闪烁后变暗，用于拍照反馈
static void anim_capturingFlash() {
    uint32_t now = millis();
    if (now - animTimer < 20) return;
    animTimer = now;
    animStep++;
    if (animStep < 5) {
        fill_solid(leds, WS2812_NUM, CRGB::White);
    } else if (animStep < 25) {
        uint8_t fade = map(animStep, 5, 25, 255, 0);
        fill_solid(leds, WS2812_NUM, CRGB(fade, fade, fade));
    } else {
        fill_solid(leds, WS2812_NUM, CRGB::Black);
    }
    FastLED.show();
}

// 红色脉冲，用于录音反馈
static void anim_recordingPulse() {
    uint32_t now = millis();
    if (now - animTimer < 30) return;
    animTimer = now;
    uint8_t brightness = beatsin8(30, 20, 255);  // 较快脉冲
    fill_solid(leds, WS2812_NUM, CRGB(brightness, 0, 0));
    FastLED.show();
}

// 橙色跑马灯，用于生成反馈
static void anim_generatingChase() {
    uint32_t now = millis();
    if (now - animTimer < 120) return;
    animTimer = now;
    fill_solid(leds, WS2812_NUM, CRGB::Black);
    // 两个点亮的 LED 追逐
    leds[animStep % WS2812_NUM]          = CRGB(255, 120, 0);
    leds[(animStep + 1) % WS2812_NUM]    = CRGB(180, 80, 0);
    animStep++;
    FastLED.show();
}

// 彩虹循环，用于播放反馈
static void anim_playingRainbow() {
    uint32_t now = millis();
    if (now - animTimer < 40) return;
    animTimer = now;
    fill_rainbow(leds, WS2812_NUM, animStep, 256 / WS2812_NUM);
    animStep += 2;
    FastLED.show();
}

// 红色闪烁，表示错误
static void anim_errorBlink() {
    uint32_t now = millis();
    if (now - animTimer < 300) return;
    animTimer = now;
    animStep++;
    if (animStep & 1) {
        fill_solid(leds, WS2812_NUM, CRGB::Red);
    } else {
        fill_solid(leds, WS2812_NUM, CRGB::Black);
    }
    FastLED.show();
}

// 绿色渐隐，表示成功
static void anim_successFlash() {
    uint32_t now = millis();
    if (now - animTimer < 40) return;
    animTimer = now;
    animStep++;
    if (animStep < 20) {
        // 在 20 步内逐渐变暗
        uint8_t fade = map(animStep, 0, 20, 255, 0);
        fill_solid(leds, WS2812_NUM, CRGB(0, fade, 0));
    } else {
        fill_solid(leds, WS2812_NUM, CRGB::Black);
    }
    FastLED.show();
}

// ---------------------------------------------------------------------------
//  主更新分发器
// ---------------------------------------------------------------------------
void strip_update() {
    switch (currentAnim) {
        case ANIM_IDLE_BREATHE:      anim_idleBreathe();      break;
        case ANIM_CAPTURING_FLASH:   anim_capturingFlash();   break;
        case ANIM_RECORDING_PULSE:   anim_recordingPulse();   break;
        case ANIM_GENERATING_CHASE:  anim_generatingChase();  break;
        case ANIM_PLAYING_RAINBOW:   anim_playingRainbow();   break;
        case ANIM_ERROR_BLINK:       anim_errorBlink();       break;
        case ANIM_SUCCESS_FLASH:     anim_successFlash();     break;
        case ANIM_NONE:
        default:
            break;
    }
}
