# main.cpp 讲解文档

对应源码：
- src/main.cpp

依赖头文件：
- include/Config.h（全局引脚、状态枚举、动画枚举）
- src/module_button.h（按键扫描 API）
- src/module_led.h（单色灯和灯带 API）
- src/module_ui.h（OLED 与蜂鸣器 API）
- src/module_audio.h（I2S 麦克风录音 API）

---

## 1. 这个文件的角色

main.cpp 是整个程序的调度中心，负责做三件事：
1. 在 setup 中初始化所有模块
2. 在 loop 中按固定顺序调用各模块的 update（非阻塞）
3. 根据按键事件决定要触发什么动作（灯效、蜂鸣器、录音、串口输出）

你可以把它理解成“导演”，真正做硬件细节的是其他模块。

---

## 2. 关键数据结构

### ButtonMapping

字段含义：
1. btnPin：按键引脚
2. ledPin：这个按键对应的单色指示灯引脚
3. name：按键名，用于串口打印
4. animation：按下后触发的灯带动画
5. toneFreq：按下后蜂鸣器频率

btnMap 数组把 5 个功能键的数据集中管理。优点是：
- 扩展时更容易
- 主循环里能写成统一逻辑，不要重复 5 份 if 代码

---

## 3. setup 启动流程

执行顺序：
1. 打开串口（115200）
2. 调用 btn_init
3. 调用 led_init
4. 调用 strip_init
5. 调用 buzzer_init
6. 调用 ui_init
7. 调用 audio_init
8. 开机灯光自检（依次点亮 5 个单色灯）
9. 播放开机音并切换到待机呼吸灯

这里有少量 delay，用于开机提示效果，属于可接受阻塞。真正运行态都在 loop 里走非阻塞。

---

## 4. loop 主循环逻辑

每次循环都按同样顺序执行：

1. 更新模块状态
- btn_update
- strip_update
- buzzer_update
- audio_update

2. 处理串口模拟按键
- 输入 1 到 5，模拟触发对应按键动作

3. 扫描每个物理按键
- justPressed：触发音效、灯效，RECORD 键开始录音
- justReleased：RECORD 键停止录音并打印统计
- isHeld：对应单色灯保持点亮

4. 如果在录音中
- 每 300ms 打印一次实时电平和样本计数

5. 刷新 OLED
- refreshOled 根据 oledDirty 控制刷新，避免过度刷新

---

## 5. RECORD 键的特殊流程

按下时：
1. 调用 audio_startRecording
2. 成功则置 recording=true，记录开始时间
3. 串口打印“录音开始”

松开时：
1. 调用 audio_stopRecording
2. recording=false
3. 恢复待机呼吸灯
4. 串口输出统计：时长、样本数、平均电平、峰值

这是一个典型“按住录音，松开停止”的设计。

---

## 6. OLED 刷新策略

refreshOled 里有两个重要点：
1. 只有 oledDirty 才刷新
2. 限制 50ms 才允许刷新一次（约 20fps）

这样可以减少 I2C 刷屏负担，也避免屏幕闪烁。

---

## 7. 常见改动入口

1. 改按键功能映射：修改 btnMap
2. 改按键音调：改 btnMap 里的 toneFreq
3. 改按键灯效：改 btnMap 里的 animation
4. 改录音日志频率：改 recordLogTimer 的 300ms
5. 改屏幕显示内容：改 refreshOled 和 module_ui.cpp

---

## 8. 新手最常见问题

1. 为什么用 millis 而不是大量 delay
- 因为要同时处理按键、灯效、蜂鸣器、录音，delay 会卡死其他任务

2. 为什么按键逻辑分 justPressed / isHeld / justReleased
- 可以把“瞬时事件”和“持续状态”分开处理，结构更清晰

3. 为什么要模块化
- main 只写调度，硬件细节放到各模块，后续维护成本更低

---

## 9. 对应头文件在这里

main.cpp 没有独立 main.h，它调用的是以下头文件 API：
- src/module_button.h
- src/module_led.h
- src/module_ui.h
- src/module_audio.h
- include/Config.h

阅读顺序建议：
1. 先看 main.cpp 理解总流程
2. 再看 module_button.cpp（输入）
3. 再看 module_led.cpp 和 module_ui.cpp（输出）
4. 最后看 module_audio.cpp（录音）
