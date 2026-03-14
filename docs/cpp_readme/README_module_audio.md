# module_audio.cpp 讲解文档

对应源码：
- src/module_audio.cpp

对应头文件：
- src/module_audio.h

依赖：
- include/Config.h（I2S 引脚、采样率、位宽、DMA 缓冲参数）
- ESP32 I2S 驱动 driver/i2s.h

---

## 1. 模块职责

这个模块负责：
1. 初始化 I2S 数字麦克风
2. 开始与停止录音状态
3. 循环读取音频采样数据
4. 把原始采样转换成可读的电平百分比
5. 输出本次录音统计（样本总数、平均电平、峰值）

这里的“录音”目前是采集与分析，还没有保存为文件。

---

## 2. 全局状态变量

1. s_inited
- 是否完成 I2S 初始化

2. s_recording
- 当前是否处于录音状态

3. s_instantLevel
- 实时电平 0 到 100

4. s_sampleCount
- 本次录音累计样本数

5. s_sumAbs
- 累计绝对幅值总和（用于算平均）

6. s_peakSample
- 本次录音峰值（最大幅值）

7. s_dmaBuf
- I2S DMA 读取缓冲区

---

## 3. 电平映射函数

levelFromAbs(absValue) 的作用是把原始幅值转为百分比：
1. 先扣掉噪声底 noiseFloor
2. 再按 scale 做线性缩放
3. 限幅到 100

这个函数是你调“灵敏度”的核心入口。

---

## 4. audio_init 初始化流程

主要配置项：
1. 模式：I2S_MODE_MASTER | I2S_MODE_RX
2. 采样率：AUDIO_SAMPLE_RATE（当前 16000）
3. 位宽：32bit slot
4. 声道：由 I2S_MIC_USE_RIGHT 决定左/右
5. 引脚：I2S_SCK_PIN / I2S_WS_PIN / I2S_SD_PIN

注意点：
1. L/R 引脚不要悬空，要和 I2S_MIC_USE_RIGHT 对齐
2. i2s_driver_install 或 i2s_set_pin 失败会串口报错

---

## 5. 开始与停止录音

audio_startRecording：
1. 检查是否初始化
2. 进入 recording 状态
3. 清零本次统计
4. 清空 DMA 缓冲

audio_stopRecording：
1. 退出 recording 状态
2. 清实时电平

---

## 6. audio_update 核心算法

每次循环做这些事：

1. 用 i2s_read 取一块数据
- 读取失败或空数据就返回

2. 遍历样本做幅值计算
- 同时尝试两种常见数据布局（24bit 左对齐路径、低 16bit 路径）
- 取幅值更大的那种，增强兼容性

3. 更新块统计
- chunkSumAbs（平均能量）
- chunkPeak（瞬时峰值）

4. 累计全局统计
- 总样本数
- 总能量
- 全程峰值

5. 计算实时电平
- mappedAvg 和 mappedPeak 组合
- 使用快起慢落平滑：
  - 说话时快速上升
  - 静音时缓慢下降

这样电平显示更接近“音量条”体验。

---

## 7. 头文件 module_audio.h 解读

导出 API：
1. audio_init
2. audio_startRecording
3. audio_stopRecording
4. audio_update
5. audio_isRecording
6. audio_getInstantLevel
7. audio_getSampleCount
8. audio_getAverageLevel
9. audio_getPeakSample

使用顺序：
1. setup 调 audio_init
2. 按键按下时调 audio_startRecording
3. loop 每帧调 audio_update
4. 按键松开时调 audio_stopRecording
5. 通过 get 接口取统计值

---

## 8. 常见参数调优

1. 电平太低
- 减小 noiseFloor
- 减小 scale

2. 电平太抖
- 增大平滑权重（Release 慢一点）

3. 电平响应慢
- 增大 Attack 权重

4. 全程 0
- 检查 L/R 引脚是否固定
- 切换 I2S_MIC_USE_RIGHT
- 检查 SCK/WS/SD 接线

---

## 9. 下一步可扩展

1. 保存 PCM 或 WAV 到 SD 卡
2. 增加自动左右声道探测
3. 增加 AGC（自动增益）
4. 在 OLED 上画实时音量条
