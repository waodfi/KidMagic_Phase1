// ============================================================================
//  KidMagic 第一阶段 — 硬件模拟器引擎
//  将 ESP32-S3 固件状态机逻辑 1:1 移植到浏览器 JavaScript
// ============================================================================

(() => {
    "use strict";

    // ============================================================================
    //  配置（对应 Config.h）
    // ============================================================================
    const SystemState = {
        BOOTING: 0, IDLE: 1, CAPTURING: 2, RECORDING: 3,
        GENERATING: 4, PLAYING: 5, ERROR: 6, STARTING: 7
    };
    const STATE_NAMES = ["BOOTING", "IDLE", "CAPTURING", "RECORDING", "GENERATING", "PLAYING", "ERROR", "STARTING"];

    const LedAnimation = {
        NONE: 0, IDLE_BREATHE: 1, CAPTURING_FLASH: 2, RECORDING_PULSE: 3,
        GENERATING_CHASE: 4, PLAYING_RAINBOW: 5, ERROR_BLINK: 6, SUCCESS_FLASH: 7
    };

    const WIFI_TIMEOUT_MS = 5000;   // 模拟器中缩短
    const HTTP_TIMEOUT_MS = 10000;
    const ERROR_DISPLAY_MS = 3000;
    const DEBOUNCE_MS = 20;

    // 按键名称和键盘映射
    const BUTTONS = [
        { id: "btnStart", name: "START", keys: ["1", "s", "S"] },
        { id: "btnPhoto", name: "PHOTO", keys: ["2", "d", "D"] },
        { id: "btnRecord", name: "RECORD", keys: ["3", "f", "F"] },
        { id: "btnGenerate", name: "GENERATE", keys: ["4", "g", "G"] },
        { id: "btnPlay", name: "PLAY", keys: ["5", "h", "H"] },
    ];

    // ============================================================================
    //  DOM 元素引用
    // ============================================================================
    const DOM = {
        oled: document.getElementById("oledCanvas"),
        stateBadge: document.getElementById("stateBadge"),
        serial: document.getElementById("serialOutput"),
        serialClear: document.getElementById("serialClear"),
        btnConnectSerial: document.getElementById("btnConnectSerial"),
        netDelay: document.getElementById("netDelay"),
        netDelayVal: document.getElementById("netDelayVal"),
        wifiOn: document.getElementById("wifiOn"),
        wifiOff: document.getElementById("wifiOff"),
        httpOk: document.getElementById("httpOk"),
        httpFail: document.getElementById("httpFail"),
        wsLeds: Array.from({ length: 8 }, (_, i) => document.getElementById(`ws${i}`)),
        btnLeds: Array.from({ length: 5 }, (_, i) => document.getElementById(`btnLed${i}`)),
        hwBtns: BUTTONS.map(b => document.getElementById(b.id)),
    };

    const oledCtx = DOM.oled.getContext("2d");

    // ============================================================================
    //  串口监视器
    // ============================================================================
    let isHardwareConnected = false;

    function serialPrint(msg) {
        const ts = new Date().toLocaleTimeString("en-US", { hour12: false });
        DOM.serial.textContent += `[${ts}] ${msg}\n`;
        DOM.serial.scrollTop = DOM.serial.scrollHeight;
    }

    DOM.serialClear.addEventListener("click", () => { DOM.serial.textContent = ""; });

    // ============================================================================
    //  millis() 等效实现
    // ============================================================================
    const startTime = performance.now();
    function millis() { return Math.floor(performance.now() - startTime); }

    // ============================================================================
    //  模块：按键
    // ============================================================================
    const btnState = BUTTONS.map(() => ({
        rawPressed: false,
        currentState: false,
        lastState: false,
        justPressedFlag: false,
        justReleasedFlag: false,
        lastChangeTime: 0,
    }));

    function btn_update() {
        const now = millis();
        for (let i = 0; i < BUTTONS.length; i++) {
            const b = btnState[i];
            b.justPressedFlag = false;
            b.justReleasedFlag = false;

            if (b.rawPressed !== b.lastState) {
                b.lastChangeTime = now;
            }
            if ((now - b.lastChangeTime) >= DEBOUNCE_MS) {
                if (b.rawPressed !== b.currentState) {
                    b.currentState = b.rawPressed;
                    if (b.rawPressed) b.justPressedFlag = true;
                    else b.justReleasedFlag = true;
                }
            }
            b.lastState = b.rawPressed;
        }
    }

    function btn_justPressed(idx) { return btnState[idx].justPressedFlag; }
    function btn_isHeld(idx) { return btnState[idx].currentState; }
    function btn_justReleased(idx) { return btnState[idx].justReleasedFlag; }

    // 输入处理（键盘 + 鼠标）
    function pressButton(idx) {
        if (!isHardwareConnected) {
            btnState[idx].rawPressed = true;
        } else {
            // 如果硬件已连接，通过串口发送模拟按键到 ESP32
            sendSerialCommand(BUTTONS[idx].keys[0]);
        }
        DOM.hwBtns[idx].classList.add("pressed");
        if (isHardwareConnected) led_set(idx, true); // 即时视觉反馈
    }

    function releaseButton(idx) {
        if (!isHardwareConnected) {
            btnState[idx].rawPressed = false;
        } else {
            if (idx === 2) { // 只有录音按键在模拟中关心松开事件
                sendSerialCommand(BUTTONS[idx].keys[0]);
            }
        }
        DOM.hwBtns[idx].classList.remove("pressed");

        // 硬件连接时，我们仅通过串口状态日志驱动 updateButtonLeds() 来控制 LED
        // 因此这里不手动清除
        if (!isHardwareConnected) {
            // 本地模拟在 loop() 中处理
        }
    }

    // 鼠标事件
    DOM.hwBtns.forEach((el, i) => {
        el.addEventListener("mousedown", (e) => { e.preventDefault(); pressButton(i); });
        el.addEventListener("mouseup", (e) => { e.preventDefault(); releaseButton(i); });
        el.addEventListener("mouseleave", (e) => { if (btnState[i].rawPressed) releaseButton(i); });
        // 触摸支持
        el.addEventListener("touchstart", (e) => { e.preventDefault(); pressButton(i); });
        el.addEventListener("touchend", (e) => { e.preventDefault(); releaseButton(i); });
    });

    // ============================================================================
    //  模块：LED（单色按键指示灯）
    // ============================================================================
    function led_set(idx, on) {
        if (on) DOM.btnLeds[idx].classList.add("on");
        else DOM.btnLeds[idx].classList.remove("on");
    }

    function led_allOff() { for (let i = 0; i < 5; i++) led_set(i, false); }
    function led_allOn() { for (let i = 0; i < 5; i++) led_set(i, true); }

    // ============================================================================
    //  模块：LED（WS2812B 灯带动画）
    // ============================================================================
    let currentAnim = LedAnimation.NONE;
    let animTimer = 0;
    let animStep = 0;

    function strip_setAnimation(anim) {
        if (anim === currentAnim) return;
        currentAnim = anim;
        animStep = 0;
        animTimer = millis();
        strip_clear();
    }

    function strip_clear() {
        DOM.wsLeds.forEach(el => {
            el.style.background = "#1a1a1a";
            el.style.boxShadow = "none";
            el.classList.remove("lit");
        });
    }

    function strip_setLed(idx, r, g, b) {
        const el = DOM.wsLeds[idx];
        el.style.background = `rgb(${r},${g},${b})`;
        const brightness = Math.max(r, g, b);
        if (brightness > 30) {
            el.style.boxShadow = `0 0 ${8 + brightness / 20}px rgba(${r},${g},${b},0.7)`;
            el.classList.add("lit");
        } else {
            el.style.boxShadow = "none";
            el.classList.remove("lit");
        }
    }

    function strip_setAll(r, g, b) {
        for (let i = 0; i < 8; i++) strip_setLed(i, r, g, b);
    }

    // beatsin8 等效实现
    function beatsin8(bpm, low, high) {
        const beat = (millis() * bpm * 256) / 60000;
        const val = Math.sin(beat * 2 * Math.PI / 256) * 0.5 + 0.5;
        return Math.floor(low + val * (high - low));
    }

    // HSV 转 RGB
    function hsvToRgb(h, s, v) {
        h = h / 255 * 360;
        s = s / 255;
        v = v / 255;
        const c = v * s;
        const x = c * (1 - Math.abs((h / 60) % 2 - 1));
        const m = v - c;
        let r, g, b;
        if (h < 60) { r = c; g = x; b = 0; }
        else if (h < 120) { r = x; g = c; b = 0; }
        else if (h < 180) { r = 0; g = c; b = x; }
        else if (h < 240) { r = 0; g = x; b = c; }
        else if (h < 300) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }
        return [Math.floor((r + m) * 255), Math.floor((g + m) * 255), Math.floor((b + m) * 255)];
    }

    function anim_idleBreathe() {
        if (millis() - animTimer < 30) return;
        animTimer = millis();
        const br = beatsin8(12, 10, 60);
        strip_setAll(0, br, br);
    }

    function anim_capturingFlash() {
        if (millis() - animTimer < 20) return;
        animTimer = millis();
        animStep++;
        if (animStep < 5) {
            strip_setAll(255, 255, 255);
        } else if (animStep < 25) {
            const fade = Math.floor(255 * (25 - animStep) / 20);
            strip_setAll(fade, fade, fade);
        } else {
            strip_setAll(0, 0, 0);
        }
    }

    function anim_recordingPulse() {
        if (millis() - animTimer < 30) return;
        animTimer = millis();
        const br = beatsin8(30, 20, 255);
        strip_setAll(br, 0, 0);
    }

    function anim_generatingChase() {
        if (millis() - animTimer < 120) return;  // Slower chase
        animTimer = millis();
        strip_setAll(0, 0, 0);
        strip_setLed(animStep % 8, 255, 120, 0);
        strip_setLed((animStep + 1) % 8, 180, 80, 0);
        animStep++;
    }

    function anim_playingRainbow() {
        if (millis() - animTimer < 40) return;
        animTimer = millis();
        for (let i = 0; i < 8; i++) {
            const hue = ((animStep + i * 32) % 256);
            const [r, g, b] = hsvToRgb(hue, 255, 200);
            strip_setLed(i, r, g, b);
        }
        animStep += 2;
    }

    function anim_errorBlink() {
        if (millis() - animTimer < 300) return;
        animTimer = millis();
        animStep++;
        if (animStep & 1) strip_setAll(255, 0, 0);
        else strip_setAll(0, 0, 0);
    }

    function anim_successFlash() {
        if (millis() - animTimer < 40) return;
        animTimer = millis();
        animStep++;
        if (animStep < 20) {
            // 平滑绿色渐隐，而非突然闪烁
            const fade = Math.max(0, 255 - animStep * 12);
            strip_setAll(0, fade, 0);
        } else {
            strip_setAll(0, 0, 0);
        }
    }

    function strip_update() {
        switch (currentAnim) {
            case LedAnimation.IDLE_BREATHE: anim_idleBreathe(); break;
            case LedAnimation.CAPTURING_FLASH: anim_capturingFlash(); break;
            case LedAnimation.RECORDING_PULSE: anim_recordingPulse(); break;
            case LedAnimation.GENERATING_CHASE: anim_generatingChase(); break;
            case LedAnimation.PLAYING_RAINBOW: anim_playingRainbow(); break;
            case LedAnimation.ERROR_BLINK: anim_errorBlink(); break;
            case LedAnimation.SUCCESS_FLASH: anim_successFlash(); break;
        }
    }

    // ============================================================================
    //  模块：UI — OLED 显示屏（Canvas 模拟）
    // ============================================================================
    const OLED_W = 128, OLED_H = 64;
    const SCALE = 4;  // 画布尺寸 512×256

    function oled_clear() {
        oledCtx.fillStyle = "#000";
        oledCtx.fillRect(0, 0, 512, 256);
    }

    function oled_drawStr(x, y, text, fontSize = 13) {
        oledCtx.fillStyle = "#00ccff";
        oledCtx.font = `${fontSize * SCALE / 2.5}px 'JetBrains Mono', monospace`;
        oledCtx.fillText(text, x * SCALE, y * SCALE);
    }

    function oled_drawHLine(x, y, w) {
        oledCtx.strokeStyle = "#00ccff";
        oledCtx.lineWidth = SCALE;
        oledCtx.beginPath();
        oledCtx.moveTo(x * SCALE, y * SCALE);
        oledCtx.lineTo((x + w) * SCALE, y * SCALE);
        oledCtx.stroke();
    }

    function oled_drawFrame(x, y, w, h) {
        oledCtx.strokeStyle = "#00ccff";
        oledCtx.lineWidth = SCALE;
        oledCtx.strokeRect(x * SCALE, y * SCALE, w * SCALE, h * SCALE);
    }

    function oled_drawBox(x, y, w, h) {
        oledCtx.fillStyle = "#00ccff";
        oledCtx.fillRect(x * SCALE, y * SCALE, w * SCALE, h * SCALE);
    }

    function ui_init() {
        oled_clear();
        oled_drawStr(20, 30, "KidMagic v1", 13);
        oled_drawStr(20, 50, "Booting...", 13);
    }

    function ui_showStatus(state, msg) {
        oled_clear();
        oled_drawStr(0, 14, "KidMagic", 14);
        oled_drawHLine(0, 17, 128);
        oled_drawStr(0, 34, "State:", 13);
        oled_drawStr(48, 34, STATE_NAMES[state] || "?", 13);
        if (msg) oled_drawStr(0, 52, msg, 13);
    }

    function ui_showProgress(percent) {
        percent = Math.min(100, Math.max(0, percent));
        oled_clear();
        oled_drawStr(0, 14, "KidMagic", 14);
        oled_drawHLine(0, 17, 128);
        oled_drawStr(0, 34, "Generating...", 13);
        oled_drawFrame(0, 42, 128, 14);
        const fw = Math.floor(124 * percent / 100);
        oled_drawBox(2, 44, fw, 10);
        oled_drawStr(52, 62, `${percent}%`, 13);
    }

    function ui_showError(msg) {
        oled_clear();
        oled_drawStr(0, 14, "!! ERROR !!", 14);
        oled_drawHLine(0, 17, 128);
        if (msg) oled_drawStr(0, 38, msg, 13);
    }

    // ============================================================================
    //  模块：UI — 蜂鸣器（Web Audio API）
    // ============================================================================
    let audioCtx = null;

    function ensureAudioCtx() {
        if (!audioCtx) {
            audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        }
        if (audioCtx.state === "suspended") audioCtx.resume();
    }

    // 旋律引擎
    let melodyQueue = [];
    let melodyPlaying = false;
    let currentOsc = null;

    function buzzer_playTone(freq, durationMs) {
        ensureAudioCtx();
        const osc = audioCtx.createOscillator();
        const gain = audioCtx.createGain();

        // 混合方波和锯齿波产生特征性的压电蜂鸣器音色
        osc.type = "sawtooth";
        osc.frequency.value = freq;

        // 硬起音、保持、快速释放（无指数衰减，避免铃/马林巴音色）
        gain.gain.setValueAtTime(0, audioCtx.currentTime);
        gain.gain.linearRampToValueAtTime(0.15, audioCtx.currentTime + 0.01);
        gain.gain.setValueAtTime(0.15, audioCtx.currentTime + durationMs / 1000 - 0.01);
        gain.gain.linearRampToValueAtTime(0, audioCtx.currentTime + durationMs / 1000);

        osc.connect(gain);
        gain.connect(audioCtx.destination);

        osc.start();
        osc.stop(audioCtx.currentTime + durationMs / 1000 + 0.05);
    }

    function buzzer_playMelody(notes) {
        ensureAudioCtx();
        let offset = 0;
        for (const note of notes) {
            if (note.freq > 0) {
                const osc = audioCtx.createOscillator();
                const gain = audioCtx.createGain();

                osc.type = "sawtooth";
                osc.frequency.value = note.freq;

                // 压电蜂鸣器包络约束：
                const startTime = audioCtx.currentTime + offset / 1000;
                const stopTime = audioCtx.currentTime + (offset + note.dur) / 1000;

                gain.gain.setValueAtTime(0, startTime);
                gain.gain.linearRampToValueAtTime(0.15, startTime + 0.005);
                gain.gain.setValueAtTime(0.15, stopTime - 0.005);
                gain.gain.linearRampToValueAtTime(0, stopTime);

                osc.connect(gain);
                gain.connect(audioCtx.destination);

                osc.start(startTime);
                osc.stop(stopTime + 0.02);
            }
            offset += note.dur;
        }
    }

    function buzzer_beepStart() {
        buzzer_playMelody([
            { freq: 523, dur: 100 }, { freq: 659, dur: 100 }, { freq: 784, dur: 100 }, { freq: 1047, dur: 200 }
        ]);
    }

    function buzzer_beepShutter() {
        buzzer_playMelody([
            { freq: 4000, dur: 30 }, { freq: 0, dur: 30 }, { freq: 3000, dur: 40 }
        ]);
    }

    function buzzer_beepSuccess() {
        buzzer_playMelody([
            { freq: 784, dur: 80 }, { freq: 988, dur: 80 }, { freq: 1175, dur: 80 }, { freq: 1568, dur: 150 }
        ]);
    }

    function buzzer_beepError() {
        buzzer_playMelody([
            { freq: 200, dur: 150 }, { freq: 0, dur: 50 }, { freq: 200, dur: 150 }
        ]);
    }

    // ============================================================================
    //  模块：网络模拟
    // ============================================================================
    let netStatus = "IDLE";   // IDLE | BUSY | SUCCESS | ERROR
    let wifiConnected = true;
    let httpShouldSucceed = true;
    let responseDelayMs = 2000;  // 默认 2 秒，以获得更好的视觉反馈

    // UI 控件
    DOM.netDelay.addEventListener("input", () => {
        responseDelayMs = parseInt(DOM.netDelay.value);
        DOM.netDelayVal.textContent = responseDelayMs + "ms";
    });

    function setToggle(activeBtn, inactiveBtn) {
        activeBtn.classList.add("active");
        inactiveBtn.classList.remove("active");
    }

    DOM.wifiOn.addEventListener("click", () => {
        wifiConnected = true;
        setToggle(DOM.wifiOn, DOM.wifiOff);
        serialPrint("[NET] WiFi connected (simulated)");
    });

    DOM.wifiOff.addEventListener("click", () => {
        wifiConnected = false;
        setToggle(DOM.wifiOff, DOM.wifiOn);
        serialPrint("[NET] WiFi disconnected (simulated)");
    });

    DOM.httpOk.addEventListener("click", () => {
        httpShouldSucceed = true;
        setToggle(DOM.httpOk, DOM.httpFail);
    });

    DOM.httpFail.addEventListener("click", () => {
        httpShouldSucceed = false;
        setToggle(DOM.httpFail, DOM.httpOk);
    });

    function net_isConnected() { return wifiConnected; }

    function net_sendCommand(endpoint) {
        if (netStatus === "BUSY") {
            serialPrint("[NET] HTTP busy, command dropped.");
            return false;
        }
        netStatus = "BUSY";
        serialPrint(`[NET] POST -> http://sim-server:8080${endpoint}`);

        setTimeout(() => {
            if (httpShouldSucceed) {
                netStatus = "SUCCESS";
                serialPrint(`[NET] HTTP 200 : {"status":"ok"}`);
            } else {
                netStatus = "ERROR";
                serialPrint(`[NET] HTTP error: Connection refused`);
            }
        }, responseDelayMs);
        return true;
    }

    function net_getStatus() { return netStatus; }
    function net_clearStatus() { netStatus = "IDLE"; }

    // ============================================================================
    //  状态机（对应 main.cpp）
    // ============================================================================
    let currentState = SystemState.BOOTING;
    let previousState = SystemState.BOOTING;
    let stateEntryTime = 0;
    let errorMsg = null;
    let bootDotTimer = 0;
    let dotCount = 0;

    function changeState(newState, errMsg = null) {
        previousState = currentState;
        currentState = newState;
        stateEntryTime = millis();
        errorMsg = errMsg;
        serialPrint(`[FSM] ${STATE_NAMES[previousState]} -> ${STATE_NAMES[newState]}`);
        updateStateBadge();
    }

    function updateStateBadge() {
        const badge = DOM.stateBadge;
        badge.textContent = STATE_NAMES[currentState];
        badge.className = "state-badge";
        switch (currentState) {
            case SystemState.ERROR: badge.classList.add("error"); break;
            case SystemState.RECORDING: badge.classList.add("recording"); break;
            case SystemState.CAPTURING:
            case SystemState.GENERATING:
            case SystemState.PLAYING: badge.classList.add("busy"); break;
        }
    }

    function updateButtonLeds() {
        // 物理按键按下时优先亮灯
        for (let i = 0; i < 5; i++) {
            led_set(i, btn_isHeld(i));
        }

        // 同时点亮当前激活状态对应的按键灯
        switch (currentState) {
            case SystemState.STARTING:
                led_set(0, true);
                break;
            case SystemState.CAPTURING:
                led_set(1, true);
                break;
            case SystemState.RECORDING:
                led_set(2, true);
                break;
            case SystemState.GENERATING:
                led_set(3, true);
                break;
            case SystemState.PLAYING:
                led_set(4, true);
                break;
        }
    }

    // --- 状态处理函数 ---
    function fsm_booting() {
        if (net_isConnected()) {
            buzzer_beepStart();
            ui_showStatus(SystemState.IDLE, "Ready!");
            strip_setAnimation(LedAnimation.IDLE_BREATHE);
            changeState(SystemState.IDLE);
            return;
        }

        if (millis() - stateEntryTime > WIFI_TIMEOUT_MS) {
            changeState(SystemState.ERROR, "WiFi timeout");
            return;
        }

        const now = millis();
        if (now - bootDotTimer > 500) {
            bootDotTimer = now;
            dotCount = (dotCount + 1) % 4;
            const dots = ".".repeat(dotCount);
            ui_showStatus(SystemState.BOOTING, "Connecting" + dots);
        }
    }

    function fsm_idle() {
        // START
        if (btn_justPressed(0)) {
            serialPrint("[FSM] START pressed");
            buzzer_playTone(1000, 50);
            net_sendCommand("/api/start");
            strip_setAnimation(LedAnimation.SUCCESS_FLASH);
            ui_showStatus(SystemState.STARTING, "Starting...");
            changeState(SystemState.STARTING);
            return;
        }

        // PHOTO
        if (btn_justPressed(1)) {
            serialPrint("[FSM] PHOTO pressed");
            buzzer_beepShutter();
            net_sendCommand("/api/capture");
            strip_setAnimation(LedAnimation.CAPTURING_FLASH);
            ui_showStatus(SystemState.CAPTURING, "Capturing...");
            changeState(SystemState.CAPTURING);
            return;
        }

        // RECORD (press = start)
        if (btn_justPressed(2)) {
            serialPrint("[FSM] RECORD pressed (start)");
            buzzer_playTone(880, 60);
            net_sendCommand("/api/record/start");
            strip_setAnimation(LedAnimation.RECORDING_PULSE);
            ui_showStatus(SystemState.RECORDING, "Recording...");
            changeState(SystemState.RECORDING);
            return;
        }

        // GENERATE
        if (btn_justPressed(3)) {
            serialPrint("[FSM] GENERATE pressed");
            buzzer_playTone(660, 50);
            net_sendCommand("/api/generate");
            strip_setAnimation(LedAnimation.GENERATING_CHASE);
            ui_showStatus(SystemState.GENERATING, "Generating...");
            changeState(SystemState.GENERATING);
            return;
        }

        // PLAY
        if (btn_justPressed(4)) {
            serialPrint("[FSM] PLAY pressed");
            buzzer_playTone(784, 50);
            net_sendCommand("/api/play");
            strip_setAnimation(LedAnimation.PLAYING_RAINBOW);
            ui_showStatus(SystemState.PLAYING, "Playing...");
            changeState(SystemState.PLAYING);
            return;
        }

        // 空闲时清除过期的网络结果
        if (net_getStatus() === "SUCCESS" || net_getStatus() === "ERROR") {
            net_clearStatus();
        }
    }

    function fsm_starting() {
        const st = net_getStatus();
        if (st === "SUCCESS") {
            net_clearStatus();
            buzzer_beepSuccess();
            strip_setAnimation(LedAnimation.SUCCESS_FLASH);
            ui_showStatus(SystemState.IDLE, "Start OK!");
            changeState(SystemState.IDLE);
        } else if (st === "ERROR") {
            changeState(SystemState.ERROR, "Start failed");
        }
    }

    function fsm_capturing() {
        const st = net_getStatus();
        if (st === "SUCCESS") {
            net_clearStatus();
            buzzer_beepSuccess();
            strip_setAnimation(LedAnimation.SUCCESS_FLASH);
            ui_showStatus(SystemState.IDLE, "Photo OK!");
            changeState(SystemState.IDLE);
        } else if (st === "ERROR") {
            changeState(SystemState.ERROR, "Capture failed");
        }
    }

    function fsm_recording() {
        if (btn_justReleased(2)) {
            serialPrint("[FSM] RECORD released (stop)");
            net_sendCommand("/api/record/stop");
            buzzer_playTone(440, 60);
            strip_setAnimation(LedAnimation.IDLE_BREATHE);
            ui_showStatus(SystemState.IDLE, "Rec stopped");
            net_clearStatus();
            changeState(SystemState.IDLE);
        }
    }

    function fsm_generating() {
        const st = net_getStatus();
        if (st === "SUCCESS") {
            net_clearStatus();
            buzzer_beepSuccess();
            strip_setAnimation(LedAnimation.SUCCESS_FLASH);
            ui_showStatus(SystemState.IDLE, "Generated!");
            changeState(SystemState.IDLE);
        } else if (st === "ERROR") {
            changeState(SystemState.ERROR, "Generate fail");
        }
    }

    function fsm_playing() {
        const st = net_getStatus();
        if (st === "SUCCESS") {
            net_clearStatus();
            strip_setAnimation(LedAnimation.IDLE_BREATHE);
            ui_showStatus(SystemState.IDLE, "Play done");
            changeState(SystemState.IDLE);
        } else if (st === "ERROR") {
            changeState(SystemState.ERROR, "Play failed");
        }
    }

    function fsm_error() {
        if (previousState !== SystemState.ERROR) {
            buzzer_beepError();
            strip_setAnimation(LedAnimation.ERROR_BLINK);
            ui_showError(errorMsg || "Unknown error");
            led_allOff();
            net_clearStatus();
            previousState = SystemState.ERROR;
        }

        if (millis() - stateEntryTime > ERROR_DISPLAY_MS) {
            strip_setAnimation(LedAnimation.IDLE_BREATHE);
            ui_showStatus(SystemState.IDLE, "Ready!");
            changeState(SystemState.IDLE);
        }
    }

    // ============================================================================
    //  Web Serial API（硬件同步）
    // ============================================================================
    let port = null;
    let writer = null;
    let reader = null;

    async function sendSerialCommand(cmd) {
        if (!writer) return;
        try {
            await writer.write(cmd);
        } catch (e) {
            console.error("Failed to write to serial", e);
        }
    }

    // 解析来自 ESP32 的传入行并触发 UI 更新
    function handleHardwareLog(line) {
        serialPrint(line);

        // 匹配 "[FSM] x -> y"
        const fsmMatch = line.match(/\[FSM\] (\d+) -> (\d+)/);
        if (fsmMatch) {
            const newState = parseInt(fsmMatch[2], 10);
            updateHardwareUiState(newState);
        }

        // 映射模拟按键事件的视觉指示
        if (line.includes("[SIM] Simulated START press")) { buzzer_playTone(1000, 50); }
        if (line.includes("[SIM] Simulated PHOTO press")) { buzzer_beepShutter(); }
        if (line.includes("[SIM] Simulated RECORD press")) { buzzer_playTone(880, 60); }
        if (line.includes("[SIM] Simulated RECORD release")) { buzzer_playTone(440, 60); }
        if (line.includes("[SIM] Simulated GENERATE press")) { buzzer_playTone(660, 50); }
        if (line.includes("[SIM] Simulated PLAY press")) { buzzer_playTone(784, 50); }

        // 成功 / 错误 提示音
        if (line.includes("Start OK") || line.includes("Photo OK") || line.includes("Generated")) {
            buzzer_beepSuccess();
        }
        if (line.includes("error") || line.includes("failed")) {
            buzzer_beepError();
        }
    }

    // 由于不运行 loop()，我们直接响应状态变化
    function updateHardwareUiState(newState) {
        currentState = newState;
        updateStateBadge();
        updateButtonLeds();

        switch (currentState) {
            case SystemState.BOOTING:
                ui_showStatus(SystemState.BOOTING, "Hardware Connecting...");
                break;
            case SystemState.IDLE:
                ui_showStatus(SystemState.IDLE, "Hardware Ready!");
                strip_setAnimation(LedAnimation.IDLE_BREATHE);
                break;
            case SystemState.STARTING:
                ui_showStatus(SystemState.STARTING, "Starting...");
                strip_setAnimation(LedAnimation.SUCCESS_FLASH);
                break;
            case SystemState.CAPTURING:
                ui_showStatus(SystemState.CAPTURING, "Capturing...");
                strip_setAnimation(LedAnimation.CAPTURING_FLASH);
                break;
            case SystemState.RECORDING:
                ui_showStatus(SystemState.RECORDING, "Recording...");
                strip_setAnimation(LedAnimation.RECORDING_PULSE);
                break;
            case SystemState.GENERATING:
                ui_showStatus(SystemState.GENERATING, "Generating...");
                strip_setAnimation(LedAnimation.GENERATING_CHASE);
                break;
            case SystemState.PLAYING:
                ui_showStatus(SystemState.PLAYING, "Playing...");
                strip_setAnimation(LedAnimation.PLAYING_RAINBOW);
                break;
            case SystemState.ERROR:
                ui_showError("Hardware Error");
                strip_setAnimation(LedAnimation.ERROR_BLINK);
                setTimeout(() => {
                    if (currentState === SystemState.ERROR) {
                        strip_setAnimation(LedAnimation.IDLE_BREATHE);
                    }
                }, ERROR_DISPLAY_MS);
                break;
        }
    }

    async function connectSerial() {
        if (!("serial" in navigator)) {
            alert("Web Serial API not supported in this browser. Please use Chrome or Edge.");
            return;
        }

        try {
            port = await navigator.serial.requestPort();
            await port.open({ baudRate: 115200 });

            // 释放 ESP32 由默认 DTR/RTS 行为引起的启动/复位状态
            try {
                await port.setSignals({ dataTerminalReady: false, requestToSend: false });
            } catch (err) {
                console.warn("Could not set DTR/RTS signals", err);
            }

            // 设置标志，使按键通过串口发送命令，并停止 loop()
            isHardwareConnected = true;
            DOM.btnConnectSerial.textContent = "🔋 硬件已连接";
            DOM.btnConnectSerial.classList.add("active");
            serialPrint("=== 已连接到 ESP32 硬件 ===");
            serialPrint("--> 提示：请现在按下 ESP32 开发板上的物理 RST 按键以重新启动。");

            // 设置写入器
            const textEncoder = new TextEncoderStream();
            const writableStreamClosed = textEncoder.readable.pipeTo(port.writable);
            writer = textEncoder.writable.getWriter();

            // 设置读取器
            const textDecoder = new TextDecoderStream();
            const readableStreamClosed = port.readable.pipeTo(textDecoder.writable);
            reader = textDecoder.readable.getReader();

            let buffer = "";
            while (true) {
                const { value, done } = await reader.read();
                if (done) break;

                buffer += value;
                let lines = buffer.split("\n");
                for (let i = 0; i < lines.length - 1; i++) {
                    const line = lines[i].trim();
                    if (line) handleHardwareLog(line);
                }
                buffer = lines[lines.length - 1];
            }
        } catch (error) {
            console.error(error);
            serialPrint(`[SYS] Serial error: ${error.message}`);
            isHardwareConnected = false;
            DOM.btnConnectSerial.textContent = "🔌 Connect USB";
            DOM.btnConnectSerial.classList.remove("active");
            writer = null;
            reader = null;
        }
    }

    if (DOM.btnConnectSerial) {
        DOM.btnConnectSerial.addEventListener("click", () => {
            if (!isHardwareConnected) {
                ensureAudioCtx();
                connectSerial();
            }
        });
    }

    // ============================================================================
    //  主循环 — 通过 requestAnimationFrame 实现 60fps
    // ============================================================================
    function loop() {
        // 无论硬件是否连接，始终运行灯带动画
        strip_update();

        // 如果硬件未连接，运行完整的逻辑模拟
        if (!isHardwareConnected) {
            btn_update();
            switch (currentState) {
                case SystemState.BOOTING: fsm_booting(); break;
                case SystemState.IDLE: fsm_idle(); break;
                case SystemState.STARTING: fsm_starting(); break;
                case SystemState.CAPTURING: fsm_capturing(); break;
                case SystemState.RECORDING: fsm_recording(); break;
                case SystemState.GENERATING: fsm_generating(); break;
                case SystemState.PLAYING: fsm_playing(); break;
                case SystemState.ERROR: fsm_error(); break;
            }
            updateButtonLeds();
        }

        requestAnimationFrame(loop);
    }

    // ============================================================================
    //  启动
    // ============================================================================
    function setup() {
        serialPrint("=== KidMagic 第一阶段模拟器 ===");
        serialPrint("[系统] 所有模块已初始化");
        serialPrint("[FSM] STATE_BOOTING");

        ui_init();
        stateEntryTime = millis();
        updateStateBadge();

        // 启动主循环
        requestAnimationFrame(loop);
    }

    // 页面加载时启动
    setup();

})();
