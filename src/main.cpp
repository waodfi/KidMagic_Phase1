// ============================================================================
//  KidMagic 第一阶段 — 硬件调试程序
//  ESP32-S3 | PlatformIO + Arduino
//
//  功能：测试 5 个按键、5 个指示灯、WS2812B 灯带、蜂鸣器、OLED 显示屏
//  无网络依赖，纯本地硬件验证
// ============================================================================

#include <Arduino.h>
#include "Config.h"
#include "module_button.h"
#include "module_led.h"
#include "module_ui.h"

// ---------------------------------------------------------------------------
//  按键名称与对应资源映射表
// ---------------------------------------------------------------------------
struct ButtonMapping {
    uint8_t      btnPin;       // 按键 GPIO
    uint8_t      ledPin;       // 指示灯 GPIO
    const char*  name;         // 按键名称
    LedAnimation animation;   // 按下时的灯带动画
    uint16_t     toneFreq;    // 按下时的蜂鸣器频率
};

static const ButtonMapping btnMap[BTN_COUNT] = {
    { BTN_START,    LED_START,    "START",    ANIM_SUCCESS_FLASH,    1000 },
    { BTN_PHOTO,    LED_PHOTO,    "PHOTO",    ANIM_CAPTURING_FLASH,  4000 },
    { BTN_RECORD,   LED_RECORD,   "RECORD",   ANIM_RECORDING_PULSE,   880 },
    { BTN_GENERATE, LED_GENERATE, "GENERATE", ANIM_GENERATING_CHASE,  660 },
    { BTN_PLAY,     LED_PLAY,     "PLAY",     ANIM_PLAYING_RAINBOW,   784 },
};

// ---------------------------------------------------------------------------
//  OLED 显示状态
// ---------------------------------------------------------------------------
static const char* lastAction   = "Ready";
static bool        recording    = false;   // RECORD 按键保持状态
static bool        oledDirty    = true;    // 标记是否需要刷新屏幕
static uint32_t    oledTimer    = 0;

// 刷新 OLED：顶部标题 + 按键状态 + 最近操作
static void refreshOled() {
    if (!oledDirty) return;
    if (millis() - oledTimer < 50) return;  // 限制刷新率 ~20fps
    oledTimer = millis();
    oledDirty = false;

    // 构建按键状态字符串，按下显示首字母，未按显示横杠
    char btnLine[22];
    snprintf(btnLine, sizeof(btnLine), "S:%c P:%c R:%c G:%c Y:%c",
        btn_isHeld(BTN_START)    ? '#' : '-',
        btn_isHeld(BTN_PHOTO)    ? '#' : '-',
        btn_isHeld(BTN_RECORD)   ? '#' : '-',
        btn_isHeld(BTN_GENERATE) ? '#' : '-',
        btn_isHeld(BTN_PLAY)     ? '#' : '-');

    ui_showStatus(STATE_IDLE, btnLine);
}

// ---------------------------------------------------------------------------
//  串口输入：1-5 模拟按键，用于无物理按键时测试
// ---------------------------------------------------------------------------
static void processSerialInput() {
    if (!Serial.available()) return;
    char c = Serial.read();
    if (c == '\n' || c == '\r') return;

    int idx = c - '1';  // '1'->0, '2'->1, ..., '5'->4
    if (idx < 0 || idx >= BTN_COUNT) {
        Serial.printf("[串口] 无效输入 '%c'，请输入 1-5\n", c);
        return;
    }

    const ButtonMapping& m = btnMap[idx];
    Serial.printf("[串口] 模拟按键: %s\n", m.name);

    // 触发音效
    if (idx == 1) {
        buzzer_beepShutter();  // PHOTO 使用快门音
    } else {
        buzzer_playTone(m.toneFreq, 60);
    }

    // 触发灯带动画
    strip_setAnimation(m.animation);

    // 闪烁对应指示灯（200ms）
    led_set(m.ledPin, true);
    lastAction = m.name;
    oledDirty = true;
}

// ============================================================================
//  Arduino setup()
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n====================================");
    Serial.println("  KidMagic 第一阶段 — 硬件调试");
    Serial.println("====================================");

    // 初始化所有模块
    btn_init();
    led_init();
    strip_init();
    buzzer_init();
    ui_init();

    // 开机自检：所有指示灯依次点亮
    Serial.println("[启动] 指示灯自检...");
    for (int i = 0; i < BTN_COUNT; i++) {
        led_set(btnMap[i].ledPin, true);
        delay(100);
    }
    delay(300);
    led_allOff();

    // 开机音效 + 呼吸灯
    buzzer_beepStart();
    strip_setAnimation(ANIM_IDLE_BREATHE);

    Serial.println("[启动] 所有模块初始化完成！");
    Serial.println("");
    Serial.println("操作方式:");
    Serial.println("  物理按键 — 按下任意按键测试对应功能");
    Serial.println("  串口输入 — 发送 1-5 模拟对应按键");
    Serial.println("    1=START  2=PHOTO  3=RECORD  4=GENERATE  5=PLAY");
    Serial.println("");
}

// ============================================================================
//  Arduino loop()
// ============================================================================
void loop() {
    // 1. 更新所有模块（非阻塞）
    btn_update();
    strip_update();
    buzzer_update();

    // 2. 处理串口模拟输入
    processSerialInput();

    // 3. 扫描 5 个物理按键
    for (int i = 0; i < BTN_COUNT; i++) {
        const ButtonMapping& m = btnMap[i];

        // --- 按下瞬间 ---
        if (btn_justPressed(m.btnPin)) {
            Serial.printf("[按键] %s 按下\n", m.name);

            // 触发音效
            if (i == 1) {
                buzzer_beepShutter();    // PHOTO 快门声
            } else {
                buzzer_playTone(m.toneFreq, 60);
            }

            // 触发灯带动画
            strip_setAnimation(m.animation);

            // RECORD 特殊处理：按下开始录音
            if (i == 2) {
                recording = true;
                lastAction = "Recording...";
            } else {
                lastAction = m.name;
            }

            oledDirty = true;
        }

        // --- 松开瞬间 ---
        if (btn_justReleased(m.btnPin)) {
            Serial.printf("[按键] %s 松开\n", m.name);

            // RECORD 特殊处理：松开停止录音
            if (i == 2 && recording) {
                recording = false;
                buzzer_playTone(440, 60);
                strip_setAnimation(ANIM_IDLE_BREATHE);
                lastAction = "Rec stopped";
                Serial.println("[按键] RECORD 录音停止");
            }

            oledDirty = true;
        }

        // 指示灯跟随按键状态（按住亮，松开灭）
        led_set(m.ledPin, btn_isHeld(m.btnPin));
    }

    // 4. 刷新 OLED 显示
    refreshOled();
}