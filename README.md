# ESP32-S3 FreeRTOS Learning

FreeRTOS projects on the ESP32-S3, built with ESP-IDF in VS Code. Each folder is a separate, working project, and each one adds new RTOS concepts on real hardware.

Projects are ordered from basics to more advanced: tasks, then ISR and queues, then sensor pipelines.

## Projects

| Folder | What it shows | RTOS concepts |
|--------|---------------|---------------|
| [02-two-tasks](02-two-tasks) | Two tasks running at the same time, one on each core | Task creation, `xTaskCreatePinnedToCore`, dual-core pinning (core 0 and core 1), blocking with `vTaskDelay` |
| [03-button-led-isr](03-button-led-isr) | Push button changes LED blink modes | GPIO interrupt (ISR), ISR-to-task communication, queue, multiple tasks |
| [04-dht11-queue](04-dht11-queue) | Reads a DHT11 sensor and logs temperature and humidity | Task pinned to a core, queue producer/consumer, critical section for a timing-sensitive read, blocking with `vTaskDelay` and `xQueueReceive` |

## Hardware

- ESP32-S3 development board
- Breadboard, LED with resistor, push button (project 03)
- DHT11 3-pin module (project 04)

## Wiring: 04-dht11-queue

| DHT11 | ESP32-S3 |
|-------|----------|
| VCC | 3V3 |
| GND | GND |
| DATA | GPIO4 |

## Sample output (04-dht11-queue)

```
I (151794) DHT_RTOS: Temp: 28.6 C  Humidity: 49.0 %
I (153814) DHT_RTOS: Temp: 28.6 C  Humidity: 49.0 %
```

## How the DHT11 project works

- `dht_task` (core 1) reads the sensor every 2 seconds and sends the result to a queue.
- `logger_task` (core 0) sleeps until data arrives, then prints it.
- The sensor read runs inside a critical section because its bit timing is measured in microseconds.

## Build and flash

1. Install ESP-IDF and the VS Code ESP-IDF extension
2. Open a project folder, for example `04-dht11-queue`
3. Set the target: `ESP-IDF: Set Espressif Device Target` and choose `esp32s3`
4. Select the COM port, then press **Build, Flash and Monitor**

## Learning roadmap

- [x] Tasks, core pinning, delays
- [x] Queues and ISR-to-task pattern
- [x] Critical sections
- [ ] Task notifications
- [ ] Semaphores and mutexes
- [ ] Software timers and event groups
- [ ] Watchdog, heap and stack debugging
- [ ] Wi-Fi and MQTT in tasks# ESP32-S3 FreeRTOS Learning

FreeRTOS projects on the ESP32-S3, built with ESP-IDF in VS Code.
Each folder is a separate project.

## Projects

| Folder | What it shows |
|---|---|
| 03-button-led-isr | Button interrupt, queue, tasks, LED blink modes |

## Hardware

ESP32-S3 board, breadboard, LED with resistor, push button
