# module_button.cpp 讲解文档

对应源码：
- src/module_button.cpp

对应头文件：
- src/module_button.h

依赖：
- include/Config.h（按键引脚、按键数量、去抖时间）

---

## 1. 模块职责

这个模块专门处理按键输入，提供三个核心状态查询：
1. btn_justPressed：这一帧刚按下
2. btn_isHeld：当前持续按住
3. btn_justReleased：这一帧刚松开

它把底层数字电平和业务事件隔离开，主程序不用关心防抖细节。

---

## 2. 内部数据结构

### ButtonState

每个按键都维护一份状态：
1. pin：按键 GPIO
2. currentState：当前稳定状态（按下或松开）
3. lastState：上一帧原始采样值
4. justPressedFlag：下降沿事件
5. justReleasedFlag：上升沿事件
6. lastChangeTime：最近一次采样变化时间

buttons 数组长度是 BTN_COUNT，对应 5 个按键。

---

## 3. 初始化流程

btn_init 做了两件事：
1. 初始化每个 ButtonState 字段
2. pinMode 设置为 INPUT_PULLUP

这里 INPUT_PULLUP 很关键，表示默认高电平，按下接地后变低电平。
所以模块里会把 LOW 判为“按下”。

---

## 4. 防抖与边沿检测

btn_update 每次 loop 调用一次，流程如下：

1. 读取原始电平
- rawPressed = digitalRead(pin) == LOW

2. 每帧清空边沿标志
- justPressedFlag = false
- justReleasedFlag = false

3. 若采样变化，记录变化时间
- rawPressed != lastState 时刷新 lastChangeTime

4. 若变化已稳定超过 DEBOUNCE_MS
- 才真正更新 currentState
- 并设置按下/松开边沿标志

5. 保存本帧采样到 lastState

这样可以过滤机械按键抖动。

---

## 5. 三个查询函数

1. btn_justPressed(pin)
- 只在按下那一帧返回 true
- 下一帧会自动清零

2. btn_isHeld(pin)
- 只要按住就持续 true

3. btn_justReleased(pin)
- 只在松开那一帧返回 true

这三个函数是主循环做事件驱动的基础。

---

## 6. 头文件 module_button.h 解读

导出 API 非常简洁：
1. btn_init
2. btn_update
3. btn_justPressed
4. btn_isHeld
5. btn_justReleased

使用约定：
1. setup 调一次 btn_init
2. loop 每次都要先调 btn_update
3. 然后再读取状态函数

如果不先 update，读到的是旧状态。

---

## 7. 常见修改点

1. 改防抖时间
- 修改 include/Config.h 的 DEBOUNCE_MS

2. 改按键数量或引脚
- 修改 include/Config.h 的 BTN_COUNT 与 BTN_xxx
- 同步检查 btnPins 数组长度

3. 想加长按检测
- 可在 ButtonState 里新增 pressStartTime
- 按下时记录，按住时计算时长

---

## 8. 调试建议

1. 串口打印 justPressed 和 justReleased，确认边沿是否丢失
2. 如果按键反了（松开判按下），先检查是否用了 INPUT_PULLUP
3. 若出现连发，通常是 DEBOUNCE_MS 太小或接线不稳
