#include "bsp_audio.h"

#include "bsp_lcd_jd9365_10_1.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

static esp_codec_dev_handle_t s_speaker;
static i2s_chan_handle_t s_i2s_tx;

#define BSP_AUDIO_I2S_PORT I2S_NUM_1
#define BSP_AUDIO_I2S_DOUT GPIO_NUM_9
#define BSP_AUDIO_I2S_BCLK GPIO_NUM_12
#define BSP_AUDIO_I2S_MCLK GPIO_NUM_13
#define BSP_AUDIO_I2S_WS   GPIO_NUM_10
#define BSP_AUDIO_PA_GPIO  GPIO_NUM_53

esp_err_t bsp_audio_speaker_init(unsigned sample_rate, unsigned volume)
{
    if (s_speaker == NULL) {
        i2c_master_bus_handle_t i2c_bus = bsp_touch_get_i2c_bus_handle();
        if (i2c_bus == NULL) {
            return ESP_ERR_INVALID_STATE;
        }

        i2s_chan_config_t channel_cfg = I2S_CHANNEL_DEFAULT_CONFIG(BSP_AUDIO_I2S_PORT, I2S_ROLE_MASTER);
        channel_cfg.auto_clear = true;
        if (i2s_new_channel(&channel_cfg, &s_i2s_tx, NULL) != ESP_OK) {
            return ESP_FAIL;
        }
        i2s_std_config_t i2s_cfg = {
            .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
            .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
            .gpio_cfg = {
                .mclk = BSP_AUDIO_I2S_MCLK,
                .bclk = BSP_AUDIO_I2S_BCLK,
                .ws = BSP_AUDIO_I2S_WS,
                .dout = BSP_AUDIO_I2S_DOUT,
                .din = I2S_GPIO_UNUSED,
            },
        };
        if (i2s_channel_init_std_mode(s_i2s_tx, &i2s_cfg) != ESP_OK ||
            i2s_channel_enable(s_i2s_tx) != ESP_OK) {
            return ESP_FAIL;
        }

        audio_codec_i2s_cfg_t data_cfg = {
            .port = BSP_AUDIO_I2S_PORT,
            .tx_handle = s_i2s_tx,
        };
        audio_codec_i2c_cfg_t i2c_cfg = {
            .port = BSP_TOUCH_I2C_PORT,
            .addr = ES8311_CODEC_DEFAULT_ADDR,
            .bus_handle = i2c_bus,
        };
        const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&data_cfg);
        const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
        const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
        if (data_if == NULL || ctrl_if == NULL || gpio_if == NULL) {
            return ESP_FAIL;
        }

        es8311_codec_cfg_t codec_cfg = {
            .ctrl_if = ctrl_if,
            .gpio_if = gpio_if,
            .codec_mode = ESP_CODEC_DEV_TYPE_OUT,
            .pa_pin = BSP_AUDIO_PA_GPIO,
            .use_mclk = true,
            .hw_gain = {.pa_voltage = 5.0, .codec_dac_voltage = 3.3},
        };
        const audio_codec_if_t *codec_if = es8311_codec_new(&codec_cfg);
        if (codec_if == NULL) {
            return ESP_FAIL;
        }
        esp_codec_dev_cfg_t dev_cfg = {
            .dev_type = ESP_CODEC_DEV_TYPE_OUT,
            .codec_if = codec_if,
            .data_if = data_if,
        };
        s_speaker = esp_codec_dev_new(&dev_cfg);
        if (s_speaker == NULL) {
            return ESP_FAIL;
        }
        esp_codec_dev_sample_info_t format = {
            .sample_rate = sample_rate,
            .channel = 2,
            .bits_per_sample = 16,
        };
        if (esp_codec_dev_open(s_speaker, &format) != ESP_CODEC_DEV_OK) {
            s_speaker = NULL;
            return ESP_FAIL;
        }
    }

    return esp_codec_dev_set_out_vol(s_speaker, volume) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t bsp_audio_speaker_write(const void *data, size_t size)
{
    if (s_speaker == NULL || data == NULL || size == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_codec_dev_write(s_speaker, (void *)data, size) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}
