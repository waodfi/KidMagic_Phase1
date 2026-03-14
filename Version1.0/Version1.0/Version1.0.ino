// ============================================================================
//  KidMagic 第一阶段 — 硬件调试程序
//  ESP32-S3 | PlatformIO + Arduino
//
//  功能：测试 5 个按键、5 个指示灯、WS2812B 灯带、蜂鸣器、OLED 显示屏
//  无网络依赖，纯本地硬件验证
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <cstring>
#include "Config.h"
#include "module_button.h"
#include "module_led.h"
#include "module_ui.h"
#include "module_mic.h"
#include "module_network.h"

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
static int         btnPressCount[BTN_COUNT] = {0}; // 记录每个按键按下的次数
static char        serialCmdBuf[32] = {0};
static uint8_t     serialCmdLen = 0;

static void printSerialHelp() {
    Serial.println("[诊断] 串口命令:");
    Serial.println("  1-5 : 模拟按键");
    Serial.println("  d   : 打印网络/按键诊断信息");
    Serial.println("  r   : 手动切换录音网络连接(开始/停止)");
    Serial.println("  h   : 显示帮助");
}

static void printDiagnostics() {
    wl_status_t st = WiFi.status();
    Serial.println("\n========== 诊断信息 ==========");
    Serial.printf("[CFG] SERVER_HOST=%s\n", SERVER_HOST);
    Serial.printf("[CFG] AUDIO_STREAM_PORT=%d\n", AUDIO_STREAM_PORT);
    Serial.printf("[CFG] TELEMETRY_PORT=%d\n", TELEMETRY_PORT);
    Serial.printf("[WIFI] status=%d, connected=%s\n", (int)st, net_isConnected() ? "YES" : "NO");
    if (st == WL_CONNECTED) {
        Serial.printf("[WIFI] localIP=%s, RSSI=%d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }
    Serial.printf("[REC] recording=%s\n", recording ? "YES" : "NO");
    Serial.printf("[BTN] START=%d PHOTO=%d RECORD=%d GENERATE=%d PLAY=%d\n",
                  btn_isHeld(BTN_START) ? 1 : 0,
                  btn_isHeld(BTN_PHOTO) ? 1 : 0,
                  btn_isHeld(BTN_RECORD) ? 1 : 0,
                  btn_isHeld(BTN_GENERATE) ? 1 : 0,
                  btn_isHeld(BTN_PLAY) ? 1 : 0);
    Serial.println("==============================\n");
}

static void executeSerialCommand(char cmd, const char* raw) {
    Serial.printf("[SER] 收到命令: %s\n", raw);

    if (cmd == 'h' || cmd == 'H') {
        printSerialHelp();
        return;
    }

    if (cmd == 'd' || cmd == 'D') {
        printDiagnostics();
        return;
    }

    if (cmd == 'r' || cmd == 'R') {
        if (!recording) {
            Serial.printf("[REC] 手动尝试连接 %s:%d ...\n", SERVER_HOST, AUDIO_STREAM_PORT);
            bool ok = net_startAudioStream(SERVER_HOST, AUDIO_STREAM_PORT);
            if (ok) {
                recording = true;
                lastAction = "Recording...";
                Serial.println("[REC] 手动连接成功，已开始录音推流");
            } else {
                Serial.println("[REC] 手动连接失败，请检查 Python 监听、端口和防火墙");
            }
        } else {
            recording = false;
            net_stopAudioStream();
            lastAction = "Rec stopped";
            Serial.println("[REC] 手动停止录音推流");
        }
        oledDirty = true;
        return;
    }

    int idx = cmd - '1';  // '1'->0, '2'->1, ..., '5'->4
    if (idx < 0 || idx >= BTN_COUNT) {
        Serial.printf("[串口] 无效输入 '%c'，请输入 1-5 / d / r / h\n", cmd);
        return;
    }

    const ButtonMapping& m = btnMap[idx];
    btnPressCount[idx]++;
    Serial.printf("[串口] 模拟按键: %s, 次数: %d\n", m.name, btnPressCount[idx]);

    // 发送串口模拟的遥测数据 (只发送按下)
    net_sendButtonTelemetry(m.name, true, btnPressCount[idx]);

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
    while (Serial.available()) {
        char c = (char)Serial.read();

        // 兼容 Arduino IDE 的三种行尾设置：无 / \n / \r\n
        if (c == '\n' || c == '\r') {
            if (serialCmdLen == 0) {
                continue;
            }

            serialCmdBuf[serialCmdLen] = '\0';
            char cmd = serialCmdBuf[0];
            char raw[32];
            strncpy(raw, serialCmdBuf, sizeof(raw));
            raw[sizeof(raw) - 1] = '\0';
            serialCmdLen = 0;
            executeSerialCommand(cmd, raw);
            continue;
        }

        // 无行尾模式时，收到单字节命令直接执行
        if (serialCmdLen == 0 && !Serial.available()) {
            char raw[2] = { c, '\0' };
            executeSerialCommand(c, raw);
        } else {
            if (serialCmdLen < sizeof(serialCmdBuf) - 1) {
                serialCmdBuf[serialCmdLen++] = c;
            } else {
                serialCmdLen = 0;
                Serial.println("[串口] 命令过长，已丢弃。请输入单字符命令。");
            }
        }
    }
}

// ============================================================================
//  Arduino setup()
// ============================================================================
void setup() {
    Serial.begin(115200); // 恢复为 115200 与串口绘图器兼容
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
    mic_init();
    net_init(); // 初始化网络

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
    Serial.printf("  录音目标 — %s:%d (TCP), 遥测端口 %d (UDP)\n", SERVER_HOST, AUDIO_STREAM_PORT, TELEMETRY_PORT);
    printSerialHelp();
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
    net_update(); // 维持网络连接状态

    // 2. 处理串口模拟输入
    processSerialInput();

    // 3. 扫描 5 个物理按键
    for (int i = 0; i < BTN_COUNT; i++) {
        const ButtonMapping& m = btnMap[i];

        // --- 按下瞬间 ---
        if (btn_justPressed(m.btnPin)) {
            btnPressCount[i]++; // 增加按键次数
            // 注意：为了录音时不发送乱码，如果现在按下的不是录音键，才打印文字
            if (i != 2) Serial.printf("[按键] %s 按下, 次数: %d\n", m.name, btnPressCount[i]);

            // 发送按键处于按下状态的遥测数据
            net_sendButtonTelemetry(m.name, true, btnPressCount[i]);

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
                
                // 开始连接电脑上的 TCP 音频服务器
                bool ok = net_startAudioStream(SERVER_HOST, AUDIO_STREAM_PORT);
                if (!ok) {
                    recording = false;
                    lastAction = "Rec start fail";
                    Serial.printf("[REC] 按键触发连接失败: %s:%d\n", SERVER_HOST, AUDIO_STREAM_PORT);
                }
                // 使用 println 让我们可以在串口绘图器里看到提示不至于太碍事
                Serial.println(0); 
            } else {
                lastAction = m.name;
            }

            oledDirty = true;
        }

        // --- 松开瞬间 ---
        if (btn_justReleased(m.btnPin)) {
            // RECORD 特殊处理：松开停止录音
            if (i == 2 && recording) {
                recording = false;
                net_stopAudioStream(); // 断开音频流网络连接
                
                buzzer_playTone(440, 60);
                strip_setAnimation(ANIM_IDLE_BREATHE);
                lastAction = "Rec stopped";
                Serial.println(0); // 发一条归零免得破坏波形
            }
            if (i != 2) Serial.printf("[按键] %s 松开\n", m.name);

            oledDirty = true;
        }

        // 指示灯跟随按键状态（按住亮，松开灭）
        led_set(m.ledPin, btn_isHeld(m.btnPin));
    }

    // 如果处于录音状态，读取音频数据并发送到电脑
    if (recording) {
        char mic_buffer[1024];
        size_t bytes_read = 0;
        mic_read_data(mic_buffer, sizeof(mic_buffer), &bytes_read);
        if (bytes_read > 0) {
            // 恢复为你最初的可以直接用的单声道解析方式
            int32_t* samples = (int32_t*)mic_buffer;
            int num_samples = bytes_read / 4;
            int16_t samples16[256]; 
            
            for (int i = 0; i < num_samples; i++) {
                // 读取原始 32 位信号，右移14位缩小为16位
                int32_t s = samples[i] >> 16; 
                
                // 限制边界防止爆音
                if (s > 32767) s = 32767;
                else if (s < -32768) s = -32768;
                samples16[i] = (int16_t)s;
                
                // 抽样打印在串口绘图器画波形（每 16 次采样打印 1 次防卡死）
                if (i % 16 == 0) {
                    Serial.println(samples16[i]);
                }
                
            }
            
            // 通过 WiFi 实时发送 16位 音频数据给电脑的 Python 服务器
            net_sendAudioData((const uint8_t*)samples16, num_samples * 2);
        }
    }

    // 4. 刷新 OLED 显示
    refreshOled();
}