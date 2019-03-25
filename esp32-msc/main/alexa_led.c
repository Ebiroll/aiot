/* Check ESP32-LyraTD-MSC board LEDs
    & speech recognition with multiple keywords.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "periph_is31fl3216.h"
#include "audio_hal.h"
#include "board.h"
#include "zl38063.h"
#include "audio_common.h"
#include "audio_pipeline.h"
#include "mp3_decoder.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "filter_resample.h"
#include "esp_sr_iface.h"
#include "esp_sr_models.h"

static const char *TAG = "CHECK_ESP32-LyraTD-MSC_LEDs";

static const char *EVENT_TAG = "asr_event";

typedef enum {
    WAKE_UP = 1,
    OPEN_THE_LIGHT,
    CLOSE_THE_LIGHT,
    VOLUME_INCREASE,
    VOLUME_DOWN,
    PLAY,
    PAUSE,
    MUTE,
    PLAY_LOCAL_MUSIC,
} asr_event_t;

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "[ 1 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PHERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[ 2 ] Initialize IS31fl3216 peripheral");
    periph_is31fl3216_cfg_t is31fl3216_cfg = { 0 };
    is31fl3216_cfg.state = IS31FL3216_STATE_ON;
    esp_periph_handle_t is31fl3216_periph = periph_is31fl3216_init(&is31fl3216_cfg);

    ESP_LOGI(TAG, "[ 3 ] Start peripherals");
    esp_periph_start(set, is31fl3216_periph);


    ESP_LOGI(TAG, "[ 4 ] Set duty for each LED index");
    for (int i = 0; i < 14; i++) {
        periph_is31fl3216_set_duty(is31fl3216_periph, i, 255);
    }


    ESP_LOGI(TAG, "Initialize SR handle");
#if CONFIG_SR_MODEL_WN4_QUANT
    const esp_sr_iface_t *model = &esp_sr_wakenet4_quantized;
#else
    const esp_sr_iface_t *model = &esp_sr_wakenet3_quantized;
#endif
    model_iface_data_t *iface = model->create(DET_MODE_90);
    int num = model->get_word_num(iface);
    for (int i = 1; i <= num; i++) {
        char *name = model->get_word_name(iface, i);
        ESP_LOGI(TAG, "keywords: %s (index = %d)", name, i);
    }
    float threshold = model->get_det_threshold_by_mode(iface, DET_MODE_90, 1);
    int sample_rate = model->get_samp_rate(iface);
    int audio_chunksize = model->get_samp_chunksize(iface);
    ESP_LOGI(EVENT_TAG, "keywords_num = %d, threshold = %f, sample_rate = %d, chunksize = %d, sizeof_uint16 = %d", num, threshold, sample_rate, audio_chunksize, sizeof(int16_t));
    int16_t *buff = (int16_t *)malloc(audio_chunksize * sizeof(short));
    if (NULL == buff) {
        ESP_LOGE(EVENT_TAG, "Memory allocation failed!");
        model->destroy(iface);
        model = NULL;
        return;
    }

    ESP_LOGI(EVENT_TAG, "[ 1 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    audio_pipeline_handle_t pipeline;
    audio_element_handle_t i2s_stream_reader, filter, raw_read;

    ESP_LOGI(EVENT_TAG, "[ 2.0 ] Create audio pipeline for recording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(EVENT_TAG, "[ 2.1 ] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.i2s_config.sample_rate = 48000;
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(EVENT_TAG, "[ 2.2 ] Create filter to resample audio data");
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 48000;
    rsp_cfg.src_ch = 2;
    rsp_cfg.dest_rate = 16000;
    rsp_cfg.dest_ch = 1;
    rsp_cfg.type = AUDIO_CODEC_TYPE_ENCODER;
    filter = rsp_filter_init(&rsp_cfg);

    ESP_LOGI(EVENT_TAG, "[ 2.3 ] Create raw to receive data");
    raw_stream_cfg_t raw_cfg = {
        .out_rb_size = 8 * 1024,
        .type = AUDIO_STREAM_READER,
    };
    raw_read = raw_stream_init(&raw_cfg);

    ESP_LOGI(EVENT_TAG, "[ 3 ] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline, filter, "filter");
    audio_pipeline_register(pipeline, raw_read, "raw");

    ESP_LOGI(EVENT_TAG, "[ 4 ] Link elements together [codec_chip]-->i2s_stream-->filter-->raw-->[SR]");
    audio_pipeline_link(pipeline, (const char *[]) {"i2s", "filter", "raw"}, 3);

    ESP_LOGI(EVENT_TAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    int led_index=1;

    while (1) {
        raw_stream_read(raw_read, (char *)buff, audio_chunksize * sizeof(short));

        int keyword = model->detect(iface, (int16_t *)buff);
        switch (keyword) {
            case WAKE_UP:
                ESP_LOGI(TAG, "Wake up");
 	 	        {
                  periph_is31fl3216_state_t led_state = IS31FL3216_STATE_ON;
			      int blink_pattern = (1UL << led_index) - 1;
			      periph_is31fl3216_set_blink_pattern(is31fl3216_periph, blink_pattern);
			      //led_state = (led_state == IS31FL3216_STATE_ON) ? IS31FL3216_STATE_FLASH : IS31FL3216_STATE_ON;
                  led_state = IS31FL3216_STATE_ON;
			      periph_is31fl3216_set_state(is31fl3216_periph, led_state);
		        }

                led_index++;
                if (led_index > 14){
                    led_index = 0;
                }


                break;
            case OPEN_THE_LIGHT:
                ESP_LOGI(TAG, "Turn on the light");
                break;
            case CLOSE_THE_LIGHT:
                ESP_LOGI(TAG, "Turn off the light");
                break;
            case VOLUME_INCREASE:
                ESP_LOGI(TAG, "volume increase");
                break;
            case VOLUME_DOWN:
                ESP_LOGI(TAG, "volume down");
                break;
            case PLAY:
                ESP_LOGI(TAG, "play");
                break;
            case PAUSE:
                ESP_LOGI(TAG, "pause");
                break;
            case MUTE:
                ESP_LOGI(TAG, "mute");
                break;
            case PLAY_LOCAL_MUSIC:
                ESP_LOGI(TAG, "play local music");
                break;
            default:
                ESP_LOGD(TAG, "Not supported keyword");
                break;
        }

    }

    ESP_LOGI(EVENT_TAG, "[ 6 ] Stop audio_pipeline");

    audio_pipeline_terminate(pipeline);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    audio_pipeline_unregister(pipeline, raw_read);
    audio_pipeline_unregister(pipeline, i2s_stream_reader);
    audio_pipeline_unregister(pipeline, filter);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(raw_read);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(filter);

#if 0
    ESP_LOGI(TAG, "[ 1 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = { 0 };
    esp_periph_init(&periph_cfg);



    ESP_LOGI(TAG, "[ 5 ] Rotate LED pattern");
    int led_index = 0;
    periph_is31fl3216_state_t led_state = IS31FL3216_STATE_ON;
    while (1) {
        int blink_pattern = (1UL << led_index) - 1;
        periph_is31fl3216_set_blink_pattern(is31fl3216_periph, blink_pattern);
        led_index++;
        if (led_index > 14){
            led_index = 0;
            led_state = (led_state == IS31FL3216_STATE_ON) ? IS31FL3216_STATE_FLASH : IS31FL3216_STATE_ON;
            periph_is31fl3216_set_state(is31fl3216_periph, led_state);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "[ 6 ] Destroy peripherals");
    esp_periph_destroy();
    ESP_LOGI(TAG, "[ 7 ] Finished");
#endif

    ESP_LOGI(EVENT_TAG, "[ 7 ] Destroy model");
    model->destroy(iface);
    model = NULL;
    free(buff);
    buff = NULL;



}
