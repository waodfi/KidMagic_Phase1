#include "module_button.h"

// ---------------------------------------------------------------------------
//  内部数据
// ---------------------------------------------------------------------------
struct ButtonState {
    uint8_t  pin;
    bool     currentState;   // true = 按下 (低电平有效，已反转)
    bool     lastState;
    bool     justPressedFlag;
    bool     justReleasedFlag;
    uint32_t lastChangeTime;
};

static ButtonState buttons[BTN_COUNT];

static const uint8_t btnPins[BTN_COUNT] = {
    BTN_START, BTN_PHOTO, BTN_RECORD, BTN_GENERATE, BTN_PLAY
};

// ---------------------------------------------------------------------------
//  通过引脚查找按键索引 (未找到则返回 -1)
// ---------------------------------------------------------------------------
static int8_t findIndex(uint8_t pin) {
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        if (buttons[i].pin == pin) return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
//  公共 API
// ---------------------------------------------------------------------------
void btn_init() {
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        buttons[i].pin             = btnPins[i];
        buttons[i].currentState    = false;
        buttons[i].lastState       = false;
        buttons[i].justPressedFlag = false;
        buttons[i].justReleasedFlag = false;
        buttons[i].lastChangeTime  = 0;
        pinMode(btnPins[i], INPUT_PULLUP);
    }
}

void btn_update() {
    uint32_t now = millis();
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        // 低电平有效：由于使用了内部上拉，低电平时表示按下
        bool rawPressed = (digitalRead(buttons[i].pin) == LOW);

        // 每帧清除边缘标志
        buttons[i].justPressedFlag  = false;
        buttons[i].justReleasedFlag = false;

        // 软件防抖
        if (rawPressed != buttons[i].lastState) {
            buttons[i].lastChangeTime = now;
        }

        if ((now - buttons[i].lastChangeTime) >= DEBOUNCE_MS) {
            if (rawPressed != buttons[i].currentState) {
                buttons[i].currentState = rawPressed;
                if (rawPressed) {
                    buttons[i].justPressedFlag = true;
                } else {
                    buttons[i].justReleasedFlag = true;
                }
            }
        }

        buttons[i].lastState = rawPressed;
    }
}

bool btn_justPressed(uint8_t pin) {
    int8_t idx = findIndex(pin);
    return (idx >= 0) ? buttons[idx].justPressedFlag : false;
}

bool btn_isHeld(uint8_t pin) {
    int8_t idx = findIndex(pin);
    return (idx >= 0) ? buttons[idx].currentState : false;
}

bool btn_justReleased(uint8_t pin) {
    int8_t idx = findIndex(pin);
    return (idx >= 0) ? buttons[idx].justReleasedFlag : false;
}
