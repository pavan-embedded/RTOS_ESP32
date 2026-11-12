# ESP32-S3 FreeRTOS Learning

| Folder | What it shows | RTOS concepts |
|--------|---------------|---------------|
| [02-two-tasks](02-two-tasks) | Two tasks running at the same time, one on each core | Task creation, `xTaskCreatePinnedToCore`, dual-core pinning (core 0 and core 1), blocking with `vTaskDelay` |
| [03-button-led-isr](03-button-led-isr) | Push button changes LED blink modes | GPIO interrupt (ISR), ISR-to-task communication, queue, multiple tasks |
| [04-dht11-queue](04-dht11-queue) | Reads a DHT11 sensor and logs temperature and humidity | Task pinned to a core, queue producer/consumer, critical section for a timing-sensitive read, blocking with `vTaskDelay` and `xQueueReceive` |
| [05-dht11-notify](05-dht11-notify) | Button press triggers an instant sensor reading | Task notifications (`vTaskNotifyGiveFromISR`, `ulTaskNotifyTake`), ISR signals a task, wait with timeout |
