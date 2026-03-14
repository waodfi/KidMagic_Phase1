# module_ui.cpp 讲解文档

对应源码：
- src/module_ui.cpp

对应头文件：
- src/module_ui.h

依赖：
- include/Config.h（OLED I2C 引脚、蜂鸣器引脚和通道、状态枚举）
- U8g2 库（OLED）
- Wire 库（I2C）

---

## 1. 模块职责

module_ui.cpp 实际上包含两部分功能：
1. OLED 显示
2. 蜂鸣器音效

这两个输出设备放在一个模块里，主程序调用更方便。

---

## 2. OLED 部分

### 2.1 对象初始化

使用 U8G2_SSD1306_128X64_NONAME_F_HW_I2C，表示：
1. SSD1306 128x64
2. 全缓冲模式（F）
3. 硬件 I2C

### 2.2 状态字符串表

stateLabels 把 SystemState 枚举映射成文本：
- BOOTING, IDLE, CAPTURING, RECORDING 等

这样 ui_showStatus 可以直接按状态号显示对应文字。

### 2.3 核心显示函数

1. ui_init
- 初始化屏幕，显示启动页

2. ui_showStatus(state, msg)
- 显示标题、状态名、可选消息

3. ui_showProgress(percent)
- 显示生成进度条和百分比

4. ui_showError(msg)
- 显示错误标题和消息
- 对超长字符串做两行拆分

---

## 3. 蜂鸣器部分

### 3.1 原理

用 ESP32 LEDC PWM 输出不同频率，驱动无源蜂鸣器发声。

### 3.2 两种播放模式

1. 单音模式
- buzzer_playTone(freq, durationMs)
- 播放固定时长后自动停止

2. 旋律模式
- startMelody 加载音符数组
- buzzer_update 按时间推进音符索引

旋律优先级高于单音，避免同时抢占。

### 3.3 预设音效

1. buzzer_beepStart：开机和弦
2. buzzer_beepShutter：拍照快门
3. buzzer_beepSuccess：成功提示
4. buzzer_beepError：错误提示

---

## 4. buzzer_update 非阻塞逻辑

每次 loop 调用时：
1. 如果在播旋律
- 判断当前音符时长是否结束
- 切下一个音符或结束静音

2. 如果不在播旋律
- 判断单音是否超时，超时则静音

这种写法不会阻塞主循环，按键和灯效还能正常响应。

---

## 5. 头文件 module_ui.h 解读

导出 API 分两组：

OLED：
1. ui_init
2. ui_showStatus
3. ui_showProgress
4. ui_showError

蜂鸣器：
1. buzzer_init
2. buzzer_update
3. buzzer_playTone
4. buzzer_beepStart
5. buzzer_beepShutter
6. buzzer_beepSuccess
7. buzzer_beepError

使用习惯：
1. setup 调 ui_init 与 buzzer_init
2. loop 每帧调 buzzer_update
3. 事件发生时调用对应显示和音效函数

---

## 6. 常见修改点

1. 改字体
- 修改 setFont 使用的字体

2. 改页面布局
- 调整 drawStr 坐标
- 调整进度条尺寸

3. 改音效
- 修改 Note 旋律数组
- 改每个音符频率和时长

4. 改蜂鸣器响度感受
- 主要受硬件和频率影响
- 可尝试不同频率组合

---

## 7. 新手注意事项

1. OLED 全缓冲模式占 RAM 更多，但绘图更灵活
2. 蜂鸣器若无声，先确认是无源蜂鸣器
3. 若画面闪烁，减少刷新频率而不是加 delay
