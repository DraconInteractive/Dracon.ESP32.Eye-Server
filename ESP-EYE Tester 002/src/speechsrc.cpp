#include "FreeRTOS.h"
#include <driver/i2s.h>
#include "Arduino.h"
#include "WebServer.h"

#include "app_speech_srcif.h"

#define I2S_SAMPLE_RATE 16000 //o=78125, *=14000
#define ADC_INPUT ADC1_CHANNEL_4 //pin 32
#define OUTPUT_PIN 40//o=27
#define OUTPUT_VALUE 3800
#define READ_DELAY 9000

QueueHandle_t sndQueue;
int audio_chunksize = 16;
static src_cfg_t srcif;
int state = 0;

void i2sInit()
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX/* | I2S_MODE_ADC_BUILT_IN*/),
        .sample_rate =  I2S_SAMPLE_RATE,              // The format of the signal using ADC_BUILT_IN
        .bits_per_sample = (i2s_bits_per_sample_t)32, // is fixed at 12bit, stereo, MSB //o=16BIT ||I2S_BITS_PER_SAMPLE_32BIT
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, //o=I2S_CHANNEL_FMT_RIGHT_LEFT, *=I2S_CHANNEL_FMT_ONLY_RIGHT
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
        .dma_buf_count = 3,
        .dma_buf_len = 300,
    };
    esp_err_t err;

    err = i2s_driver_install((i2s_port_t)1, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("Failed installing driver: %d\n", err);
    }

    static const i2s_pin_config_t pin_config = {
        .bck_io_num = 26,
        .ws_io_num = 32,
        .data_out_num = -1,
        .data_in_num = 33
    };

    err = i2s_set_pin((i2s_port_t)1, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("Failed setting pin: %d\n", err);
    }
/*
    i2s_set_adc_mode(ADC_UNIT_1, ADC_INPUT);
    err = i2s_adc_enable(I2S_NUM_0);
    Serial.printf("ADC ENABLE: %s\n", esp_err_to_name(err));
    Serial.println();*/

    i2s_zero_dma_buffer((i2s_port_t)1);
}

void recsrcTask(void *arg) {
    i2sInit();

    src_cfg_t *cfg = (src_cfg_t*)arg;
    size_t samp_len = cfg->item_size*2*sizeof(int)/sizeof(int16_t);

    int *samp=(int *)malloc(samp_len);

    size_t read_len = 0;

    while(1) {
        if (state == 1)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        i2s_read((i2s_port_t)1, samp, samp_len, &read_len, (i2s_port_t)portMAX_DELAY);
        for (int x=0; x<cfg->item_size/4; x++) {
            int s1 = ((samp[x * 4] + samp[x * 4 + 1]) >> 13) & 0x0000FFFF;
            int s2 = ((samp[x * 4 + 2] + samp[x * 4 + 3]) << 3) & 0xFFFF0000;
            samp[x] = s1 | s2;
        }

        xQueueSend(*cfg->queue, samp, portMAX_DELAY);
    }

    vTaskDelete(NULL);
}

int16_t* data;

void nnTask (void *arg) {
    int16_t *buffer=(int16_t *)malloc(audio_chunksize*sizeof(int16_t*));
    assert(buffer);

    while(1) {
        xQueueReceive(sndQueue, buffer, portMAX_DELAY);

        data = buffer;
    }

    free(buffer);
    vTaskDelete(NULL);
}

void micSetup () {
    sndQueue=xQueueCreate(2, (audio_chunksize*sizeof(int16_t)));
    srcif.queue=&sndQueue;
    srcif.item_size=audio_chunksize*sizeof(int16_t);

    xTaskCreatePinnedToCore(&recsrcTask, "rec", 3*1024, (void*)&srcif, 5, NULL, 1);
    xTaskCreatePinnedToCore(&nnTask, "nn", 2*1024, NULL, 5, NULL, 1);
}

void micLoop() {

}

int16_t* micGet () {
    return data;   
}

void GetAudioStream(WiFiClient &client, WebServer &server) {
    String response = "";
    while (1) {
        if (!client.connected()) {
            break;
        }
        response = "--frame\r\n";
        response += "Content-Type: audio/mpeg\r\n\r\n";
        server.sendContent(response);

        client.write((char *)data, 128);
        server.sendContent("\r\n");
        if (!client.connected()) {
        break;
        }
    }
}

