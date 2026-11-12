#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

#define DHT_GPIO GPIO_NUM_4

static const char *TAG = "DHT_RTOS";

typedef struct {
    float temperature;
    float humidity;
} dht_reading_t;

static QueueHandle_t dht_queue;
static portMUX_TYPE dht_mux = portMUX_INITIALIZER_UNLOCKED;

// Wait while the pin stays at 'level'. Returns elapsed us, or -1 on timeout.
static int wait_while(int level, int timeout_us)
{
    int t = 0;
    while (gpio_get_level(DHT_GPIO) == level) {
        if (t++ >= timeout_us) return -1;
        esp_rom_delay_us(1);
    }
    return t;
}

static esp_err_t dht11_read(float *temp, float *hum)
{
    uint8_t data[5] = {0};

    // Start signal: pull low >= 18 ms (outside critical section, we can sleep)
    gpio_set_level(DHT_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    esp_err_t err = ESP_OK;
    portENTER_CRITICAL(&dht_mux);

    gpio_set_level(DHT_GPIO, 1);                 // release the line
    if (wait_while(1, 100) < 0) { err = ESP_ERR_TIMEOUT; goto done; } // sensor pulls low
    if (wait_while(0, 100) < 0) { err = ESP_ERR_TIMEOUT; goto done; } // 80us low
    if (wait_while(1, 100) < 0) { err = ESP_ERR_TIMEOUT; goto done; } // 80us high

    for (int i = 0; i < 40; i++) {
        if (wait_while(0, 70) < 0)  { err = ESP_ERR_TIMEOUT; goto done; } // 50us low
        int high = wait_while(1, 100);
        if (high < 0)               { err = ESP_ERR_TIMEOUT; goto done; }
        data[i / 8] <<= 1;
        if (high > 40) data[i / 8] |= 1;         // ~70us = 1, ~27us = 0
    }

done:
    portEXIT_CRITICAL(&dht_mux);
    if (err != ESP_OK) return err;

    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
        return ESP_ERR_INVALID_CRC;

    *hum  = data[0] + data[1] * 0.1f;
    *temp = data[2] + data[3] * 0.1f;
    return ESP_OK;
}

static void dht_task(void *arg)
{
    dht_reading_t r;
    while (1) {
        esp_err_t err = dht11_read(&r.temperature, &r.humidity);
        if (err == ESP_OK) {
            // Don't block forever if the logger is stuck
            if (xQueueSend(dht_queue, &r, pdMS_TO_TICKS(100)) != pdTRUE)
                ESP_LOGW(TAG, "Queue full, reading dropped");
        } else {
            ESP_LOGE(TAG, "DHT11 read failed: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(2000));         // DHT11 needs >= 1s between reads
    }
}

static void logger_task(void *arg)
{
    dht_reading_t r;
    while (1) {
        if (xQueueReceive(dht_queue, &r, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Temp: %.1f C  Humidity: %.1f %%", r.temperature, r.humidity);
        }
    }
}

void app_main(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << DHT_GPIO,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,       // open-drain, readable
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    gpio_set_level(DHT_GPIO, 1);

    dht_queue = xQueueCreate(5, sizeof(dht_reading_t));
    configASSERT(dht_queue);

    xTaskCreatePinnedToCore(dht_task,    "dht_task",    4096, NULL, 5, NULL, 1);
    //xTaskCreatePinnedToCore(logger_task, "logger_task", 4096, NULL, 4, NULL, 0);
}