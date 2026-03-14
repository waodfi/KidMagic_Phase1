#include "module_audio.h"
#include <driver/i2s.h>

static bool s_inited = false;
static bool s_recording = false;

static uint8_t s_instantLevel = 0;   // 0-100
static uint32_t s_sampleCount = 0;
static uint64_t s_sumAbs = 0;
static uint16_t s_peakSample = 0;

static int32_t s_dmaBuf[AUDIO_DMA_BUF_LEN];

static inline uint8_t levelFromAbs(uint32_t absValue) {
    // 简单噪声门限 + 线性映射，提升人声可见度
    const uint32_t noiseFloor = 20;
    const uint32_t scale = 80;
    if (absValue <= noiseFloor) return 0;

    uint32_t v = (absValue - noiseFloor) / scale;
    if (v > 100) v = 100;
    return (uint8_t)v;
}

void audio_init() {
    if (s_inited) return;

        const i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = AUDIO_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    #if I2S_MIC_USE_RIGHT
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    #else
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    #endif
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = AUDIO_DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    const i2s_pin_config_t pinConfig = {
        .bck_io_num = I2S_SCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD_PIN
    };

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, nullptr);
    if (err != ESP_OK) {
        Serial.printf("[I2S] driver install failed: %d\n", (int)err);
        return;
    }

    err = i2s_set_pin(I2S_NUM_0, &pinConfig);
    if (err != ESP_OK) {
        Serial.printf("[I2S] set pin failed: %d\n", (int)err);
        return;
    }

    i2s_zero_dma_buffer(I2S_NUM_0);
    s_inited = true;
    Serial.printf("[I2S] 麦克风初始化完成 (slot=%dbit, channel=%s)\n",
        AUDIO_BITS,
        I2S_MIC_USE_RIGHT ? "RIGHT" : "LEFT");
}

bool audio_startRecording() {
    if (!s_inited) return false;

    s_recording = true;
    s_instantLevel = 0;
    s_sampleCount = 0;
    s_sumAbs = 0;
    s_peakSample = 0;
    i2s_zero_dma_buffer(I2S_NUM_0);
    return true;
}

void audio_stopRecording() {
    s_recording = false;
    s_instantLevel = 0;
}

void audio_update() {
    if (!s_recording) return;

    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_NUM_0, s_dmaBuf, sizeof(s_dmaBuf), &bytesRead, pdMS_TO_TICKS(20));
    if (err != ESP_OK || bytesRead == 0) {
        return;
    }

    uint32_t sampleN = bytesRead / sizeof(int32_t);
    uint32_t chunkSumAbs = 0;
    uint16_t chunkPeak = 0;

    for (uint32_t i = 0; i < sampleN; i++) {
        int32_t raw = s_dmaBuf[i];

        // 兼容两种常见数据布局:
        // 1) 24bit 左对齐到 32bit slot (常见 INMP441) -> 转成近似 16bit
        // 2) 低 16bit 有效值路径
        int32_t v1 = (raw >> 8) >> 8;
        int32_t v2 = (int16_t)(raw & 0xFFFF);
        int32_t v = (abs(v1) >= abs(v2)) ? v1 : v2;

        int32_t av = (v < 0) ? -v : v;
        if (av > 32767) av = 32767;
        uint16_t a = (uint16_t)av;
        chunkSumAbs += a;
        if (a > chunkPeak) chunkPeak = a;
    }

    s_sampleCount += sampleN;
    s_sumAbs += chunkSumAbs;
    if (chunkPeak > s_peakSample) s_peakSample = chunkPeak;

    // 将 16-bit 振幅映射到 0-100，综合平均值与峰值，避免“基本为 0 偶发突刺”
    uint32_t chunkAvg = (sampleN > 0) ? (chunkSumAbs / sampleN) : 0;
    uint8_t mappedAvg = levelFromAbs(chunkAvg);
    uint8_t mappedPeak = levelFromAbs(chunkPeak);
    uint8_t mapped = (mappedAvg > (mappedPeak / 2)) ? mappedAvg : (mappedPeak / 2);

    // Attack 快、Release 慢: 说话时快速抬升，停声后缓慢下降
    if (mapped > s_instantLevel) {
        s_instantLevel = (uint8_t)((s_instantLevel * 3 + mapped * 7) / 10);
    } else {
        s_instantLevel = (uint8_t)((s_instantLevel * 9 + mapped) / 10);
    }
}

bool audio_isRecording() {
    return s_recording;
}

uint8_t audio_getInstantLevel() {
    return s_instantLevel;
}

uint32_t audio_getSampleCount() {
    return s_sampleCount;
}

uint8_t audio_getAverageLevel() {
    if (s_sampleCount == 0) return 0;
    uint32_t avgAbs = (uint32_t)(s_sumAbs / s_sampleCount);
    return levelFromAbs(avgAbs);
}

uint16_t audio_getPeakSample() {
    return s_peakSample;
}
