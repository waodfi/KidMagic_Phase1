#include "module_ui.h"
#include <U8g2lib.h>
#include <Wire.h>

// ============================================================================
//  OLED 显示屏 (SSD1306, 128x64, I2C)
// ============================================================================
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

static const char* stateLabels[] = {
    "BOOTING",      // STATE_BOOTING
    "IDLE",         // STATE_IDLE
    "CAPTURING",    // STATE_CAPTURING
    "RECORDING",    // STATE_RECORDING
    "GENERATING",   // STATE_GENERATING
    "PLAYING",      // STATE_PLAYING
    "ERROR",        // STATE_ERROR
    "STARTING"      // STATE_STARTING
};

void ui_init() {
    u8g2.begin();
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.clearBuffer();
    u8g2.drawStr(20, 30, "KidMagic v1");
    u8g2.drawStr(20, 50, "Booting...");
    u8g2.sendBuffer();
}

void ui_showStatus(SystemState state, const char* msg) {
    u8g2.clearBuffer();

    // 标题栏
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.drawStr(0, 14, "KidMagic");
    u8g2.drawHLine(0, 17, 128);

    // 状态标签
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(0, 34, "State:");
    if ((uint8_t)state < sizeof(stateLabels)/sizeof(stateLabels[0])) {
        u8g2.drawStr(48, 34, stateLabels[(uint8_t)state]);
    }

    // 可选消息文本
    if (msg != nullptr) {
        u8g2.drawStr(0, 52, msg);
    }

    u8g2.sendBuffer();
}

void ui_showProgress(uint8_t percent) {
    if (percent > 100) percent = 100;
    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.drawStr(0, 14, "KidMagic");
    u8g2.drawHLine(0, 17, 128);

    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(0, 34, "Generating...");

    // 进度条边框
    u8g2.drawFrame(0, 42, 128, 14);
    // 填充
    uint8_t fillWidth = (uint8_t)((uint16_t)124 * percent / 100);
    u8g2.drawBox(2, 44, fillWidth, 10);

    // 百分比文本
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", percent);
    u8g2.drawStr(52, 62, buf);

    u8g2.sendBuffer();
}

void ui_showError(const char* msg) {
    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.drawStr(0, 14, "!! ERROR !!");
    u8g2.drawHLine(0, 17, 128);

    u8g2.setFont(u8g2_font_6x13_tf);
    if (msg != nullptr) {
        // 长消息自动换行，分两行显示
        size_t len = strlen(msg);
        if (len <= 21) {
            u8g2.drawStr(0, 38, msg);
        } else {
            char line1[22];
            strncpy(line1, msg, 21);
            line1[21] = '\0';
            u8g2.drawStr(0, 34, line1);
            u8g2.drawStr(0, 50, msg + 21);
        }
    }

    u8g2.sendBuffer();
}

// ============================================================================
//  蜂鸣器 (ESP32 LEDC PWM, 非阻塞旋律引擎)
// ============================================================================
struct Note {
    uint16_t freq;       // 频率 (Hz)，0 = 休止符
    uint16_t durationMs;
};

#define MAX_MELODY_LEN 8

static Note melodyBuf[MAX_MELODY_LEN];
static uint8_t  melodyLen     = 0;
static uint8_t  melodyIdx     = 0;
static bool     melodyPlaying = false;
static uint32_t noteStartTime = 0;

// 单音状态
static bool     toneActive    = false;
static uint32_t toneEndTime   = 0;

void buzzer_init() {
    // ESP32 LEDC 设置：通道、频率 (Hz)、分辨率 (位)
    ledcSetup(BUZZER_CHANNEL, 2000, 8);
    ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
    ledcWriteTone(BUZZER_CHANNEL, 0);  // 静音
}

void buzzer_playTone(uint16_t freq, uint16_t durationMs) {
    if (freq == 0) {
        ledcWriteTone(BUZZER_CHANNEL, 0);
    } else {
        ledcWriteTone(BUZZER_CHANNEL, freq);
    }
    toneActive  = true;
    toneEndTime = millis() + durationMs;
}

// ---------------------------------------------------------------------------
//  旋律播放（内部函数）
// ---------------------------------------------------------------------------
static void startMelody(const Note* notes, uint8_t len) {
    if (len > MAX_MELODY_LEN) len = MAX_MELODY_LEN;
    memcpy(melodyBuf, notes, len * sizeof(Note));
    melodyLen     = len;
    melodyIdx     = 0;
    melodyPlaying = true;
    // 播放第一个音符
    if (melodyBuf[0].freq > 0) {
        ledcWriteTone(BUZZER_CHANNEL, melodyBuf[0].freq);
    } else {
        ledcWriteTone(BUZZER_CHANNEL, 0);
    }
    noteStartTime = millis();
}

// ---------------------------------------------------------------------------
//  预设旋律
// ---------------------------------------------------------------------------
void buzzer_beepStart() {
    static const Note melody[] = {
        {523, 100}, {659, 100}, {784, 100}, {1047, 200}  // C5 E5 G5 C6
    };
    startMelody(melody, 4);
}

void buzzer_beepShutter() {
    static const Note melody[] = {
        {4000, 30}, {0, 30}, {3000, 40}   // 快门声
    };
    startMelody(melody, 3);
}

void buzzer_beepSuccess() {
    static const Note melody[] = {
        {784, 80}, {988, 80}, {1175, 80}, {1568, 150}  // G5 B5 D6 G6
    };
    startMelody(melody, 4);
}

void buzzer_beepError() {
    static const Note melody[] = {
        {200, 150}, {0, 50}, {200, 150}   // 低频警告音
    };
    startMelody(melody, 3);
}

// ---------------------------------------------------------------------------
//  非阻塞更新 — 每次 loop() 调用
// ---------------------------------------------------------------------------
void buzzer_update() {
    uint32_t now = millis();

    // 处理旋律播放
    if (melodyPlaying) {
        if (now - noteStartTime >= melodyBuf[melodyIdx].durationMs) {
            melodyIdx++;
            if (melodyIdx >= melodyLen) {
                // 旋律播放完成
                melodyPlaying = false;
                ledcWriteTone(BUZZER_CHANNEL, 0);
                return;
            }
            // 播放下一个音符
            if (melodyBuf[melodyIdx].freq > 0) {
                ledcWriteTone(BUZZER_CHANNEL, melodyBuf[melodyIdx].freq);
            } else {
                ledcWriteTone(BUZZER_CHANNEL, 0);
            }
            noteStartTime = now;
        }
        return;  // 旋律优先
    }

    // 处理单音自动停止
    if (toneActive && now >= toneEndTime) {
        ledcWriteTone(BUZZER_CHANNEL, 0);
        toneActive = false;
    }
}
