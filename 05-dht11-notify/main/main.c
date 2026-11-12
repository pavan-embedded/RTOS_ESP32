#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

#define DHT_GPIO    GPIO_NUM_4
#define BUTTON_GPIO GPIO_NUM_0      // BOOT button on the board

static const char *TAG = "DHT_RTOS";

typedef struct {
    float temperature;
    float humidity;
} dht_reading_t;

static QueueHandle_t dht_queue;
static TaskHandle_t  dht_task_handle;
static portMUX_TYPE  dht_mux = portMUX_INITIALIZER_UNLOCKED;

// ISR: only signal the task, do no real work here
static void IRAM_ATTR button_isr(void *arg)
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(dht_task_handle, &woken);
    portYIELD_FROM_ISR(woken);
}

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

    gpio_set_level(DHT_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    esp_err_t err = ESP_OK;
    portENTER_CRITICAL(&dht_mux);

    gpio_set_level(DHT_GPIO, 1);
    if (wait_while(1, 100) < 0) { err = ESP_ERR_TIMEOUT; goto done; }
    if (wait_while(0, 100) < 0) { err = ESP_ERR_TIMEOUT; goto done; }
    if (wait_while(1, 100) < 0) { err = ESP_ERR_TIMEOUT; goto done; }

    for (int i = 0; i < 40; i++) {
        if (wait_while(0, 70) < 0)  { err = ESP_ERR_TIMEOUT; goto done; }
        int high = wait_while(1, 100);
        if (high < 0)               { err = ESP_ERR_TIMEOUT; goto done; }
        data[i / 8] <<= 1;
        if (high > 40) data[i / 8] |= 1;
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
    const char *source = "startup";

    while (1) {
        ESP_LOGI(TAG, "Reading triggered by: %s", source);

        esp_err_t err = dht11_read(&r.temperature, &r.humidity);
        if (err == ESP_OK) {
            if (xQueueSend(dht_queue, &r, pdMS_TO_TICKS(100)) != pdTRUE)
                ESP_LOGW(TAG, "Queue full, reading dropped");
        } else {
            ESP_LOGE(TAG, "DHT11 read failed: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));   // DHT11 needs >= 1 s between reads

        // Sleep up to 1 s more; a button press wakes us immediately
        uint32_t presses = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        source = presses ? "button" : "timer";
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
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    gpio_set_level(DHT_GPIO, 1);

    dht_queue = xQueueCreate(5, sizeof(dht_reading_t));
    configASSERT(dht_queue);

    xTaskCreatePinnedToCore(dht_task,    "dht_task",    4096, NULL, 5, &dht_task_handle, 1);
    xTaskCreatePinnedToCore(logger_task, "logger_task", 4096, NULL, 4, NULL, 0);

    // Set up the button AFTER the task exists, because the ISR uses dht_task_handle
    gpio_config_t btn = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&btn);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr, NULL);
}