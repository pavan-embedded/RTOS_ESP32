#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#define LED_GPIO 4
#define BTN_GPIO 5

static const char *TAG = "button_led";

static QueueHandle_t isr_queue;    // ISR -> button task
static QueueHandle_t mode_queue;   // button task -> LED task

static void IRAM_ATTR button_isr(void *arg)
{
    uint32_t pin = (uint32_t)arg;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(isr_queue, &pin, &woken);
    portYIELD_FROM_ISR(woken);
}

static void button_task(void *arg)
{
    uint32_t pin;
    int64_t last_ms = 0;
    int mode = 0;

    while (1) {
        if (xQueueReceive(isr_queue, &pin, portMAX_DELAY) == pdTRUE) {
            int64_t now_ms = esp_timer_get_time() / 1000;
            if (now_ms - last_ms < 200) continue;   // debounce: ignore bounces
            last_ms = now_ms;

            mode = (mode + 1) % 3;
            ESP_LOGI(TAG, "Button pressed -> mode %d", mode);
            xQueueOverwrite(mode_queue, &mode);
        }
    }
}

static void led_task(void *arg)
{
    int mode = 0, level = 0, new_mode;
    TickType_t wait = portMAX_DELAY;   // mode 0: sleep until a new mode arrives

    while (1) {
        if (xQueueReceive(mode_queue, &new_mode, wait) == pdTRUE) {
            mode = new_mode;
            level = 0;
            gpio_set_level(LED_GPIO, 0);
        } else {
            level = !level;            // timeout = time to toggle
            gpio_set_level(LED_GPIO, level);
        }
        if (mode == 0)      wait = portMAX_DELAY;
        else if (mode == 1) wait = pdMS_TO_TICKS(500);
        else                wait = pdMS_TO_TICKS(100);
    }
}

void app_main(void)
{
    gpio_config_t led_cfg = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_cfg);
    gpio_set_level(LED_GPIO, 0);

    gpio_config_t btn_cfg = {
        .pin_bit_mask = 1ULL << BTN_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,   // press = pin goes HIGH -> LOW
    };
    gpio_config(&btn_cfg);

    isr_queue  = xQueueCreate(10, sizeof(uint32_t));
    mode_queue = xQueueCreate(1, sizeof(int));

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_GPIO, button_isr, (void *)BTN_GPIO);

    xTaskCreate(button_task, "button", 3072, NULL, 10, NULL);
    xTaskCreate(led_task,    "led",    3072, NULL, 5,  NULL);
}