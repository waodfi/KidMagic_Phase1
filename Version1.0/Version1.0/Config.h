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
#define LED_GENERATE    11  // Orange
#define LED_PLAY        12  // Purple

// --- WS2812B 呼吸灯带 ----------------------------------------------------------
#define WS2812_PIN      14
#define WS2812_NUM      8
#define WS2812_MAX_BRIGHTNESS  60

// --- OLED 显示屏 (I2C) -------------------------------------------------------------
#define OLED_SDA        4
#define OLED_SCL        5

// --- 麦克风(I2S) -------------------------------------------------------------
#define I2S_MIC_WS      21      //  
#define I2S_MIC_SD      2      // 
#define I2S_MIC_SCK     1       // 
#define I2S_MIC_PORT I2S_NUM_0

// --- 蜂鸣器 (LEDC PWM) ------------------------------------------------------
#define BUZZER_PIN      15
#define BUZZER_CHANNEL  0

// --- WiFi 配置 -------------------------------------------------------------------
#define WIFI_SSID       "xbotpark_Guest"    // 替换为你的 WiFi 名字
#define WIFI_PASSWORD   "sslrobot123*"     // 替换为你的 WiFi 密码
#define WIFI_TIMEOUT_MS 15000               // WiFi 连接超时时间（毫秒）

// --- 后端服务器 ---------------------------------------------------------
#define SERVER_HOST     " 192.168.127.1"     // 替换为你的服务器 IP 地址
#define SERVER_PORT     8080                // 替换为你的服务器端口
#define AUDIO_STREAM_PORT 8085              // Python TCP 音频接收端口
#define TELEMETRY_PORT   8086               // Python UDP 遥测接收端口
#define API_START       "/api/start"        // 替换为你的服务器 API 端点
#define API_CAPTURE     "/api/capture"      // 替换为你的服务器 API 端点
#define API_RECORD_START "/api/record/start"    // 替换为你的服务器 API 端点
#define API_RECORD_STOP  "/api/record/stop"     // 替换为你的服务器 API 端点
#define API_GENERATE    "/api/generate"         // 替换为你的服务器 API 端点
#define API_PLAY        "/api/play"             // 替换为你的服务器 API 端点

// --- HTTP 设置 -------------------------------------------------------------------
#define HTTP_TIMEOUT_MS 10000               // HTTP 请求超时时间（毫秒）

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
