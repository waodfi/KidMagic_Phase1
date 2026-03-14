# module_led.cpp 讲解文档

对应源码：
- src/module_led.cpp

对应头文件：
- src/module_led.h

依赖：
- include/Config.h（单色灯引脚、灯带引脚、亮度、动画枚举）
- FastLED 库

---

## 1. 模块职责

本模块管理两类灯：
1. 5 个单色按键指示灯
2. 1 条 WS2812B 灯带动画

并且灯带动画是非阻塞设计，靠 millis 节拍驱动。

---

## 2. 单色灯部分

核心函数：
1. led_init：初始化所有单色灯为输出并默认熄灭
2. led_set(pin, on)：按引脚控制单个灯
3. led_allOff：全部关闭
4. led_allOn：全部点亮

注意：
- 当前硬件是 HIGH 点亮、LOW 熄灭
- 如果你换了低电平点亮电路，需要反转逻辑

---

## 3. 灯带状态机

内部变量：
1. leds[]：每颗 WS2812 的颜色缓存
2. currentAnim：当前动画类型（LedAnimation）
3. animTimer：动画节拍定时器
4. animStep：动画步进计数

工作方式：
1. strip_setAnimation 负责切换动画和重置状态
2. strip_update 每次 loop 调用，根据 currentAnim 分发到对应动画函数

---

## 4. 初始化与自检

strip_init 做了这些事：
1. FastLED.addLeds 绑定灯带类型、引脚和颜色顺序
2. FastLED.setBrightness 设置全局亮度上限
3. 上电自检：白灯 500ms 后熄灭

这个自检对排查硬件问题很有帮助：
- 若上电白灯都不亮，优先检查供电、地线、数据线方向

---

## 5. 各动画函数解释

1. anim_idleBreathe
- 青色呼吸
- beatsin8 生成平滑亮度曲线

2. anim_capturingFlash
- 拍照闪光
- 先全白短闪，再逐步淡出

3. anim_recordingPulse
- 录音脉冲
- 红色随节拍起伏

4. anim_generatingChase
- 生成跑马
- 两个橙色点在灯带上追逐

5. anim_playingRainbow
- 播放彩虹
- fill_rainbow 按步进滚动色相

6. anim_errorBlink
- 错误红闪
- 红黑交替

7. anim_successFlash
- 成功绿闪
- 绿色渐隐

所有动画都用 millis 节流，无需 delay。

---

## 6. 头文件 module_led.h 解读

导出 API：
1. led_init
2. led_set
3. led_allOff
4. led_allOn
5. strip_init
6. strip_setAnimation
7. strip_update

主程序使用建议：
1. setup 中调用 led_init 和 strip_init
2. loop 中每次调用 strip_update
3. 事件触发时调用 strip_setAnimation

---

## 7. 常见修改点

1. 改灯带颜色顺序
- FastLED.addLeds 的 GRB 可能要改成 RGB 或 BRG

2. 改亮度
- 修改 WS2812_MAX_BRIGHTNESS

3. 改动画速度
- 调每个动画里的时间门限（如 20ms、40ms、120ms）

4. 新增动画
- 在 Config.h 增加枚举
- 在 module_led.cpp 新增函数
- 在 strip_update 的 switch 增加分支

---

## 8. 调试建议

1. strip_setAnimation 切换后先清黑再 show，避免残影
2. 如果灯带偶尔不亮，先做 strip_init 的白光自检
3. 如果颜色不对，先排查颜色顺序和电源压降
