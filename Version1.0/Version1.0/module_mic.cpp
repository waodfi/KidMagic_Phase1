#include "module_mic.h"
#include "Config.h"
#include <driver/i2s_std.h>

static i2s_chan_handle_t rx_chan = NULL;

void mic_init() {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &rx_chan);
    if (err != ESP_OK) {
        Serial.println("I2S channel creation failed.");
        return;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        // 恢复为你最初可以正常使用的 MSB 协议与单声道模式
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)I2S_MIC_SCK,
            .ws   = (gpio_num_t)I2S_MIC_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = (gpio_num_t)I2S_MIC_SD,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    //强制绑定到左声道的设置
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    err = i2s_channel_init_std_mode(rx_chan, &std_cfg);
    if (err != ESP_OK) {
        Serial.println("I2S channel init failed.");
        return;
    }

    err = i2s_channel_enable(rx_chan);
    if (err != ESP_OK) {
        Serial.println("I2S channel enable failed.");
        return;
    }

    Serial.println("ICS-43434 I2S Microphone initialized successfully!");
}

void mic_read_data(char *buffer, size_t buffer_len, size_t *bytes_read) {
    if (rx_chan == NULL) return;
    esp_err_t result = i2s_channel_read(rx_chan, buffer, buffer_len, bytes_read, portMAX_DELAY);

    if (result != ESP_OK) {
        Serial.println("Error reading I2S data.");
    }
}
