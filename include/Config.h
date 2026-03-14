#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
//  KidMagic Phase 1 — 全局配置
// ============================================================================

// --- 按键引脚 (内部上拉，低电平有效) ---------------------------------
#define BTN_START       6
#define BTN_PHOTO       7
#define BTN_RECORD      16
#define BTN_GENERATE    17
#define BTN_PLAY        18

// --- 按键指示灯引脚 (NPN驱动，高电平点亮) --------------------------------
#define LED_START       8   // Green
#define LED_PHOTO       9   // Blue
#define LED_RECORD      10  // Red
#define LED_GENERATE    11  // Yellow
#define LED_PLAY        12  // White

// --- WS2812B 呼吸灯带 ----------------------------------------------------------
#define WS2812_PIN      14
#define WS2812_NUM      8
#define WS2812_MAX_BRIGHTNESS  60

// --- OLED 显示屏 (I2C) -------------------------------------------------------------
#define OLED_SDA        4
#define OLED_SCL        5

// --- 蜂鸣器 (LEDC PWM) ------------------------------------------------------
#define BUZZER_PIN      15
#define BUZZER_CHANNEL  0

// --- I2S 麦克风 (录音) ------------------------------------------------------
// 接线: SCK -> GPIO35, WS -> GPIO36, SD -> GPIO37
#define I2S_SCK_PIN         35
#define I2S_WS_PIN          36
#define I2S_SD_PIN          37

// 通道选择: 0=左声道, 1=右声道
// 若实时电平一直为 0，可先改成 1 再测试（很多 I2S 麦克风通过 L/R 引脚决定输出到左/右声道）
#define I2S_MIC_USE_RIGHT   1

#define AUDIO_SAMPLE_RATE   16000
#define AUDIO_BITS          32
#define AUDIO_DMA_BUF_LEN   256

// --- WiFi 配置 -------------------------------------------------------------------
#define WIFI_SSID       "YOUR_SSID"
#define WIFI_PASSWORD   "YOUR_PASSWORD"
#define WIFI_TIMEOUT_MS 15000

// --- 后端服务器 ---------------------------------------------------------
#define SERVER_HOST     "192.168.1.100"
#define SERVER_PORT     8080
#define API_START       "/api/start"
#define API_CAPTURE     "/api/capture"
#define API_RECORD_START "/api/record/start"
#define API_RECORD_STOP  "/api/record/stop"
#define API_GENERATE    "/api/generate"
#define API_PLAY        "/api/play"

// --- HTTP 设置 -------------------------------------------------------------------
#define HTTP_TIMEOUT_MS 10000

// --- 时间设置 -----------------------------------------------------------------
#define DEBOUNCE_MS     20
#define ERROR_DISPLAY_MS 3000
#define BTN_COUNT       5

// ============================================================================
//  系统状态机
// ============================================================================
enum SystemState {
    STATE_BOOTING,
    STATE_IDLE,
    STATE_CAPTURING,
    STATE_RECORDING,
    STATE_GENERATING,
    STATE_PLAYING,
    STATE_ERROR,
    STATE_STARTING
};

// ============================================================================
//  LED 动画类型
// ============================================================================
enum LedAnimation {
    ANIM_NONE,
    ANIM_IDLE_BREATHE,
    ANIM_CAPTURING_FLASH,
    ANIM_RECORDING_PULSE,
    ANIM_GENERATING_CHASE,
    ANIM_PLAYING_RAINBOW,
    ANIM_ERROR_BLINK,
    ANIM_SUCCESS_FLASH
};

#endif // CONFIG_H
