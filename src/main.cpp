// ============================================================================
//  KidMagic 第一阶段 — 主状态机
//  ESP32-S3 | PlatformIO + Arduino
// ============================================================================

#include <Arduino.h>
#include "Config.h"
#include "module_button.h"
#include "module_led.h"
#include "module_ui.h"
#include "module_network.h"

// ---------------------------------------------------------------------------
//  全局状态
// ---------------------------------------------------------------------------
static SystemState currentState  = STATE_BOOTING;
static SystemState previousState = STATE_BOOTING;
static uint32_t    stateEntryTime = 0;
static const char* errorMsg       = nullptr;

// ---------------------------------------------------------------------------
//  辅助函数：切换到新状态
// ---------------------------------------------------------------------------
static void changeState(SystemState newState, const char* errMsg = nullptr) {
    previousState  = currentState;
    currentState   = newState;
    stateEntryTime = millis();
    errorMsg       = errMsg;

    Serial.printf("[FSM] %d -> %d\n", previousState, newState);
}

// ---------------------------------------------------------------------------
//  根据当前状态映射按键指示灯
// ---------------------------------------------------------------------------
static void updateButtonLeds() {
    // 物理按键按下时优先亮灯
    led_set(LED_START, btn_isHeld(BTN_START));
    led_set(LED_PHOTO, btn_isHeld(BTN_PHOTO));
    led_set(LED_RECORD, btn_isHeld(BTN_RECORD));
    led_set(LED_GENERATE, btn_isHeld(BTN_GENERATE));
    led_set(LED_PLAY, btn_isHeld(BTN_PLAY));

    // 同时点亮当前激活状态对应的按键灯
    switch (currentState) {
        case STATE_STARTING:   led_set(LED_START, true); break;
        case STATE_CAPTURING:  led_set(LED_PHOTO, true); break;
        case STATE_RECORDING:  led_set(LED_RECORD, true); break;
        case STATE_GENERATING: led_set(LED_GENERATE, true); break;
        case STATE_PLAYING:    led_set(LED_PLAY, true); break;
        default: break;
    }
}

// ============================================================================
//  状态机处理函数
// ============================================================================

static void fsm_booting() {
    // 等待 WiFi 连接
    if (net_isConnected()) {
        buzzer_beepStart();
        ui_showStatus(STATE_IDLE, "Ready!");
        strip_setAnimation(ANIM_IDLE_BREATHE);
        changeState(STATE_IDLE);
        return;
    }

    // WiFi 超时 - 回退到离线空闲模式用于测试
    if (millis() - stateEntryTime > WIFI_TIMEOUT_MS) {
        Serial.println("[FSM] WiFi 超时，进入离线空闲模式。");
        buzzer_beepStart();
        ui_showStatus(STATE_IDLE, "Offline Ready");
        strip_setAnimation(ANIM_IDLE_BREATHE);
        changeState(STATE_IDLE);
        return;
    }

    // 等待时播放动画
    static uint32_t bootDotTimer = 0;
    static uint8_t  dotCount = 0;
    if (millis() - bootDotTimer > 500) {
        bootDotTimer = millis();
        dotCount = (dotCount + 1) % 4;
        char buf[20];
        snprintf(buf, sizeof(buf), "Connecting%.*s", dotCount, "...");
        ui_showStatus(STATE_BOOTING, buf);
    }
}

static void fsm_idle() {
    // --- 启动按键 ---
    if (btn_justPressed(BTN_START)) {
        Serial.println("[FSM] START pressed");
        buzzer_playTone(1000, 50);
        net_sendCommand(API_START);
        strip_setAnimation(ANIM_SUCCESS_FLASH);
        ui_showStatus(STATE_STARTING, "Starting...");
        changeState(STATE_STARTING);
        return;
    }

    // --- 拍照按键 ---
    if (btn_justPressed(BTN_PHOTO)) {
        Serial.println("[FSM] PHOTO pressed");
        buzzer_beepShutter();
        net_sendCommand(API_CAPTURE);
        strip_setAnimation(ANIM_CAPTURING_FLASH);
        ui_showStatus(STATE_CAPTURING, "Capturing...");
        changeState(STATE_CAPTURING);
        return;
    }

    // --- 录音按键 (按下 = 开始录音) ---
    if (btn_justPressed(BTN_RECORD)) {
        Serial.println("[FSM] RECORD pressed (start)");
        buzzer_playTone(880, 60);
        net_sendCommand(API_RECORD_START);
        strip_setAnimation(ANIM_RECORDING_PULSE);
        ui_showStatus(STATE_RECORDING, "Recording...");
        changeState(STATE_RECORDING);
        return;
    }

    // --- 生成按键 ---
    if (btn_justPressed(BTN_GENERATE)) {
        Serial.println("[FSM] GENERATE pressed");
        buzzer_playTone(660, 50);
        net_sendCommand(API_GENERATE);
        strip_setAnimation(ANIM_GENERATING_CHASE);
        ui_showStatus(STATE_GENERATING, "Generating...");
        changeState(STATE_GENERATING);
        return;
    }

    // --- 播放按键 ---
    if (btn_justPressed(BTN_PLAY)) {
        Serial.println("[FSM] PLAY pressed");
        buzzer_playTone(784, 50);
        net_sendCommand(API_PLAY);
        strip_setAnimation(ANIM_PLAYING_RAINBOW);
        ui_showStatus(STATE_PLAYING, "Playing...");
        changeState(STATE_PLAYING);
        return;
    }

    // 空闲时清除过期的网络结果
    if (net_getStatus() == NET_SUCCESS || net_getStatus() == NET_ERROR) {
        net_clearStatus();
    }
}

static void fsm_starting() {
    NetStatus st = net_getStatus();
    if (st == NET_SUCCESS) {
        net_clearStatus();
        buzzer_beepSuccess();
        strip_setAnimation(ANIM_SUCCESS_FLASH);
        ui_showStatus(STATE_IDLE, "Start OK!");
        changeState(STATE_IDLE);
    } else if (st == NET_ERROR) {
        changeState(STATE_ERROR, "Start failed");
    }
}

static void fsm_capturing() {
    NetStatus st = net_getStatus();
    if (st == NET_SUCCESS) {
        net_clearStatus();
        buzzer_beepSuccess();
        strip_setAnimation(ANIM_SUCCESS_FLASH);
        ui_showStatus(STATE_IDLE, "Photo OK!");
        changeState(STATE_IDLE);
    } else if (st == NET_ERROR) {
        changeState(STATE_ERROR, "Capture failed");
    }
}

static void fsm_recording() {
    // 录音按键松开 = 停止录音
    if (btn_justReleased(BTN_RECORD)) {
        Serial.println("[FSM] RECORD released (stop)");
        net_sendCommand(API_RECORD_STOP);
        buzzer_playTone(440, 60);
        strip_setAnimation(ANIM_IDLE_BREATHE);
        ui_showStatus(STATE_IDLE, "Rec stopped");
        net_clearStatus();
        changeState(STATE_IDLE);
        return;
    }
}

static void fsm_generating() {
    NetStatus st = net_getStatus();
    if (st == NET_SUCCESS) {
        net_clearStatus();
        buzzer_beepSuccess();
        strip_setAnimation(ANIM_SUCCESS_FLASH);
        ui_showStatus(STATE_IDLE, "Generated!");
        changeState(STATE_IDLE);
    } else if (st == NET_ERROR) {
        changeState(STATE_ERROR, "Generate fail");
    }
}

static void fsm_playing() {
    NetStatus st = net_getStatus();
    if (st == NET_SUCCESS) {
        net_clearStatus();
        strip_setAnimation(ANIM_IDLE_BREATHE);
        ui_showStatus(STATE_IDLE, "Play done");
        changeState(STATE_IDLE);
    } else if (st == NET_ERROR) {
        changeState(STATE_ERROR, "Play failed");
    }
}

static void fsm_error() {
    // 进入错误状态时
    if (previousState != STATE_ERROR) {
        buzzer_beepError();
        strip_setAnimation(ANIM_ERROR_BLINK);
        ui_showError(errorMsg ? errorMsg : "Unknown error");
        led_allOff();
        net_clearStatus();
        // 标记上一状态为 ERROR 以避免重复触发
        previousState = STATE_ERROR;
    }

    // ERROR_DISPLAY_MS 时间后自动恢复
    if (millis() - stateEntryTime > ERROR_DISPLAY_MS) {
        strip_setAnimation(ANIM_IDLE_BREATHE);
        ui_showStatus(STATE_IDLE, "Ready!");
        changeState(STATE_IDLE);
    }
}

// ============================================================================
//  Arduino setup() 与 loop() 主函数
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n=== KidMagic 第一阶段 硬件测试 (按键+LED+蜂鸣器+灯带) ===");

    // 初始化启动按键和指示灯
    pinMode(BTN_START, INPUT_PULLUP);
    pinMode(LED_START, OUTPUT);
    digitalWrite(LED_START, LOW); // LOW = 熄灭

    // 初始化拍照按键和指示灯
    pinMode(BTN_PHOTO, INPUT_PULLUP);
    pinMode(LED_PHOTO, OUTPUT);
    digitalWrite(LED_PHOTO, LOW); // LOW = 熄灭

    strip_init();
    buzzer_init();

    Serial.println("硬件测试就绪。");
    Serial.println("按 START (GPIO6) 切换绿色 LED (GPIO8)，触发成功音效和闪烁。");
    Serial.println("按 PHOTO (GPIO7) 切换蓝色 LED (GPIO9)，触发快门音效和闪烁。");

    bool lastStart = false;
    bool lastPhoto = false;

    // 仅用于硬件测试的阻塞循环（会阻止 FSM 运行）
    while (true) {
        strip_update(); // 保持动画运行
        buzzer_update(); // 保持音频运行/停止

        // 读取启动按键（低电平有效，因为使用了内部上拉）
        bool startPressed = (digitalRead(BTN_START) == LOW);
        // 写入 LED（高电平点亮，因为阳极连接到 GPIO）
        digitalWrite(LED_START, startPressed ? HIGH : LOW);
        
        // 仅在按下瞬间触发音效和灯带
        if (startPressed && !lastStart) {
            buzzer_playTone(1000, 50);
            strip_setAnimation(ANIM_SUCCESS_FLASH);
        }
        lastStart = startPressed;

        // 读取拍照按键
        bool photoPressed = (digitalRead(BTN_PHOTO) == LOW);
        digitalWrite(LED_PHOTO, photoPressed ? HIGH : LOW);

        if (photoPressed && !lastPhoto) {
            buzzer_beepShutter();
            strip_setAnimation(ANIM_CAPTURING_FLASH);
        }
        lastPhoto = photoPressed;
        
        delay(10);
    }

    // net_init();
    // stateEntryTime = millis();
    // Serial.println("[FSM] STATE_BOOTING");
}

// ============================================================================
//  串口模拟输入（用于无外设测试）
// ============================================================================
static void processSerialInput() {
    if (!Serial.available()) return;
    char c = Serial.read();
    
    // 忽略换行符
    if (c == '\n' || c == '\r') return;

    if (currentState == STATE_IDLE) {
        if (c == '1') {
            Serial.println("\n[SIM] Simulated START press");
            buzzer_playTone(1000, 50);
            net_sendCommand(API_START);
            strip_setAnimation(ANIM_SUCCESS_FLASH);
            ui_showStatus(STATE_STARTING, "Starting...");
            changeState(STATE_STARTING);
        } else if (c == '2') {
            Serial.println("\n[SIM] Simulated PHOTO press");
            buzzer_beepShutter();
            net_sendCommand(API_CAPTURE);
            strip_setAnimation(ANIM_CAPTURING_FLASH);
            ui_showStatus(STATE_CAPTURING, "Capturing...");
            changeState(STATE_CAPTURING);
        } else if (c == '3') {
            Serial.println("\n[SIM] Simulated RECORD press (start)");
            buzzer_playTone(880, 60);
            net_sendCommand(API_RECORD_START);
            strip_setAnimation(ANIM_RECORDING_PULSE);
            ui_showStatus(STATE_RECORDING, "Recording...");
            changeState(STATE_RECORDING);
        } else if (c == '4') {
            Serial.println("\n[SIM] Simulated GENERATE press");
            buzzer_playTone(660, 50);
            net_sendCommand(API_GENERATE);
            strip_setAnimation(ANIM_GENERATING_CHASE);
            ui_showStatus(STATE_GENERATING, "Generating...");
            changeState(STATE_GENERATING);
        } else if (c == '5') {
            Serial.println("\n[SIM] Simulated PLAY press");
            buzzer_playTone(784, 50);
            net_sendCommand(API_PLAY);
            strip_setAnimation(ANIM_PLAYING_RAINBOW);
            ui_showStatus(STATE_PLAYING, "Playing...");
            changeState(STATE_PLAYING);
        }
    } else if (currentState == STATE_RECORDING && c == '3') {
        // 录音时再次按 3 模拟松开按键
        Serial.println("\n[SIM] Simulated RECORD release (stop)");
        net_sendCommand(API_RECORD_STOP);
        buzzer_playTone(440, 60);
        strip_setAnimation(ANIM_IDLE_BREATHE);
        ui_showStatus(STATE_IDLE, "Rec stopped");
        net_clearStatus();
        changeState(STATE_IDLE);
    } else {
        Serial.printf("\n[SIM] Ignored input '%c' in state %d\n", c, currentState);
    }
}

void loop() {
    processSerialInput();

    // 1. 更新所有模块（非阻塞）
    btn_update();
    strip_update();
    buzzer_update();
    net_update();

    // 2. 状态机分发
    switch (currentState) {
        case STATE_BOOTING:    fsm_booting();    break;
        case STATE_IDLE:       fsm_idle();       break;
        case STATE_STARTING:   fsm_starting();   break;
        case STATE_CAPTURING:  fsm_capturing();  break;
        case STATE_RECORDING:  fsm_recording();  break;
        case STATE_GENERATING: fsm_generating(); break;
        case STATE_PLAYING:    fsm_playing();    break;
        case STATE_ERROR:      fsm_error();      break;
    }

    // 3. 更新按键指示灯以反映当前状态
    updateButtonLeds();
}