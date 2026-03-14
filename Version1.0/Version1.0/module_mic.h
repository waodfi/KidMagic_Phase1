#ifndef MODULE_MIC_H
#define MODULE_MIC_H

#include <Arduino.h>
#include "Config.h"
// 初始化麦克风
void mic_init();

// 读取一定数量的音频样本
// buffer: 用于存储数据的缓冲数组
// buffer_len: 期望读取的字节数
// bytes_read: 实际读取的字节数
void mic_read_data(char *buffer, size_t buffer_len, size_t *bytes_read);

#endif // MODULE_MIC_H