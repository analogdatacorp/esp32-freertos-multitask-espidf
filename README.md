# esp32-freertos-multitask-espidf

FreeRTOS multi-task demo on ESP32-S3 using ESP-IDF v5.x — LED blink, sensor read, and UART log running simultaneously with `xTaskCreate()`, `vTaskDelay()`, and Queue-based inter-task communication.

> **Full article + deep dive → [analogdata.blog](https://analogdata.blog)**  
> **Hands-on workshop → [edgeai.analogdata.io](https://edgeai.analogdata.io)**  
> **Full ESP-IDF curriculum → [build.analogdata.io](https://build.analogdata.io)**

---

## What This Repo Demonstrates

Most ESP32 tutorials run everything inside a single Arduino `loop()` — one task, sequential, blocking. This repo shows how to use ESP-IDF and FreeRTOS the right way:

| Demo | What it shows |
|---|---|
| `multitask_demo` | 3 independent FreeRTOS tasks running simultaneously |
| `queue_demo` | Safe inter-task data sharing using FreeRTOS Queues |

---

## Hardware Required

| Component | Details |
|---|---|
| Board | Seeed Studio XIAO ESP32-S3 |
| Cable | USB-C |
| LED | Onboard LED (GPIO 21) — no external hardware needed |

Any ESP32 or ESP32-S3 variant works. For other boards, update `LED_PIN` and the `idf.py set-target` command accordingly.

---

## Prerequisites

| Tool | Version | Install |
|---|---|---|
| ESP-IDF | v5.x (recommended v5.3) | [Installation Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/index.html) |
| Python | 3.8+ | Bundled with ESP-IDF installer |
| Git | Any recent version | [git-scm.com](https://git-scm.com) |
| CMake | 3.16+ | Bundled with ESP-IDF installer |

---

## ESP-IDF Installation (Quick)

# Use the official ESP-IDF Installer

Follow the instructions on the official ESP-IDF Installer page to install ESP-IDF. The installer will automatically download and set up the necessary tools for your development environment. You can copy paste the following link to install ESP-IDF or click on the image below to visit the official ESP-IDF Installer page.

 https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/get-started/index.html

[![ESP-IDF Installer](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/_static/espressif-logo.svg)](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/get-started/index.html)



> **Tip:** Follow the Instruction Properly As Mentioneed in the [ESP-IDF Installation](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) section of the ESP-IDF documentation.

---

## Project Structure

```
esp32-freertos-multitask-espidf/
│
├── multitask_demo/              ← Demo 1: 3 tasks running simultaneously
│   ├── CMakeLists.txt
│   └── main/
│       ├── CMakeLists.txt
│       └── main.c               ← Read this first
│
├── queue_demo/                  ← Demo 2: Producer/Consumer with FreeRTOS Queue
│   ├── CMakeLists.txt
│   └── main/
│       ├── CMakeLists.txt
│       └── main.c               ← Read after multitask_demo
│
├── LICENSE
└── README.md
```

---

## Demo 1 — Multi-Task (`multitask_demo`)

Three independent tasks running on the same ESP32-S3:

| Task | Function | Stack | Priority | Interval |
|---|---|---|---|---|
| `led_blink_task` | Toggles onboard LED | 2048 bytes | 3 (highest) | Every 500ms |
| `sensor_read_task` | Simulated sensor read (replace with real ADC/I2C) | 2048 bytes | 2 | Every 200ms |
| `uart_log_task` | Logs heap size and uptime | 4096 bytes | 1 (lowest) | Every 1 second |

### Why These Stack Sizes?

```
led_blink_task   → 2048  Simple GPIO operations only. No string formatting.
sensor_read_task → 2048  Simple arithmetic + ESP_LOGI with one format arg.
uart_log_task    → 4096  ESP_LOGI with three format args (%d, %lu, %lu)
                          uses more stack. Always go larger for logging tasks.
```

### Why These Priorities?

```
led_blink_task   → 3  LED timing should be consistent — give it highest priority
sensor_read_task → 2  Sensor reads are important but can tolerate brief delays
uart_log_task    → 1  Logging is best-effort — lowest priority is correct here
```

> **Rule:** Never assign Priority 0 — that is reserved for the FreeRTOS IDLE task.

### Code Walkthrough

#### Includes

```c
#include "freertos/FreeRTOS.h"   // Core FreeRTOS — must always be first
#include "freertos/task.h"       // xTaskCreate(), vTaskDelay(), TaskHandle_t
#include "driver/gpio.h"         // gpio_config(), gpio_set_level()
#include "esp_log.h"             // ESP_LOGI(), ESP_LOGW(), ESP_LOGE()
#include "esp_system.h"          // esp_get_free_heap_size()
```

#### GPIO Configuration

```c
gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << LED_PIN),  // Bitmask: select GPIO 21
    .mode         = GPIO_MODE_OUTPUT,   // Set as digital output
    .pull_up_en   = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_DISABLE,  // No interrupt on this pin
};
gpio_config(&io_conf);
```

`gpio_config_t` is a struct. You only set what you need — the rest defaults to 0. Always call `gpio_config()` once before using a GPIO pin in ESP-IDF.

#### The Critical Difference — `vTaskDelay()` vs `delay()`

```c
// Arduino
delay(500);
// CPU does NOTHING for 500ms. All other tasks wait.

// FreeRTOS ✅
vTaskDelay(pdMS_TO_TICKS(500));
// This task sleeps for 500ms. Scheduler runs other tasks immediately.
```

`pdMS_TO_TICKS(ms)` converts milliseconds to FreeRTOS ticks correctly based on `configTICK_RATE_HZ`. Always use it — never hardcode raw tick counts.

#### Replacing the Simulated Sensor

In `sensor_read_task`, the simulation block is clearly marked:

```c
// ── SIMULATED SENSOR READ ─────────────────────────────────────────
// Replace this block with your real sensor read, for example:
//   int sensor_value = adc1_get_raw(ADC1_CHANNEL_0);        // ADC
//   i2c_master_read_slave(i2c_num, data, len);              // I2C
//   spi_device_transmit(spi_handle, &transaction);          // SPI
sensor_value = (sensor_value + 1) % 100;
// ─────────────────────────────────────────────────────────────────
```

Replace only this block. The task structure, `vTaskDelay()`, and logging stay the same.

#### Monitoring Free Heap

```c
ESP_LOGI(TAG,
         "[STATUS] Log #%d | Heap Free: %lu bytes | Uptime: %lu ms",
         log_count++,
         (unsigned long)esp_get_free_heap_size(),
         (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS));
```

`esp_get_free_heap_size()` returns free internal heap in bytes. Watch this value in the monitor — if it keeps dropping over time, you have a memory leak somewhere.

#### `app_main()` — Entry Point

```c
void app_main(void) {
    xTaskCreate(led_blink_task,   "LED_Blink",   2048, NULL, 3, NULL);
    xTaskCreate(sensor_read_task, "Sensor_Read", 2048, NULL, 2, NULL);
    xTaskCreate(uart_log_task,    "UART_Log",    4096, NULL, 1, NULL);
    // app_main() returns — FreeRTOS scheduler takes over
}
```

In ESP-IDF, `app_main()` replaces `main()`. Once all tasks are created and `app_main()` returns, the FreeRTOS scheduler runs your tasks indefinitely.

### Build & Flash

```bash
cd multitask_demo

# Set target
idf.py set-target esp32s3

# Build
idf.py build

# Flash + open monitor (replace port with yours)
idf.py -p /dev/ttyACM0 flash monitor
```

### Finding Your Port

```bash
# Linux
ls /dev/ttyUSB* /dev/ttyACM*

# macOS
ls /dev/cu.usbmodem* /dev/cu.usbserial*

# Windows
# Device Manager → Ports (COM & LPT)
```

### Expected Output

```
I (320) MULTITASK: === FreeRTOS Multi-Task Demo ===
I (325) MULTITASK: Chip: ESP32-S3 | Cores: 2 | FreeRTOS Tick: 100 Hz
I (330) MULTITASK: LED Blink Task started — Core 0
I (335) MULTITASK: Sensor Read Task started — Core 0
I (340) MULTITASK: UART Log Task started — Core 0
I (345) MULTITASK: All 3 tasks created — FreeRTOS scheduler now running
I (350) MULTITASK: [SENSOR] Value: 1°C (simulated)
I (550) MULTITASK: [SENSOR] Value: 2°C (simulated)
I (750) MULTITASK: [SENSOR] Value: 3°C (simulated)
I (850) MULTITASK: [STATUS] Log #0 | Heap Free: 284320 bytes | Uptime: 850 ms
I (950) MULTITASK: [SENSOR] Value: 4°C (simulated)
```

### Exit Monitor

```
Ctrl + ]
```

---

## Demo 2 — Queue (`queue_demo`)

Producer task writes sensor readings into a FreeRTOS Queue every 200ms. Consumer task blocks until data is available, then processes it. No global variables. No data races.

### Code Walkthrough

#### `read_sensor()` and `update_display()`

These two functions are clearly marked as replaceable stubs:

```c
// ── Simulated sensor read ──────────────────────────────────────────────
// Replace this with your actual ADC / I2C / SPI read
int read_sensor(void) {
    static int simulated_value = 0;
    simulated_value = (simulated_value + 1) % 100;
    return simulated_value;
}

// ── Simulated display update ───────────────────────────────────────────
// Replace this with your actual OLED / LCD / TFT update call
void update_display(int value) {
    ESP_LOGI(TAG, "[DISPLAY] Showing value: %d", value);
    // e.g. ssd1306_draw_int(value);
    // e.g. lvgl_label_set_text(label, value);
}
```

These functions exist to keep the Queue pattern readable. In a real project:
- `read_sensor()` → your actual driver call (ADC, I2C, SPI)
- `update_display()` → your display library call (SSD1306, ST7789, LVGL)

The Queue mechanics around them don't change at all.

#### Queue Creation

```c
// xQueueCreate(queue_length, item_size_in_bytes)
// Creates a queue that holds 10 integers — thread-safe by design
sensor_queue = xQueueCreate(10, sizeof(int));

if (sensor_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create queue — halting");
    return;  // Always check — xQueueCreate returns NULL if heap is exhausted
}
```

Always check the return value of `xQueueCreate()`.

#### Producer — Sending Data

```c
// xQueueSend(queue, pointer_to_data, ticks_to_wait)
// pdMS_TO_TICKS(10) = wait up to 10ms if queue is full before giving up
if (xQueueSend(sensor_queue, &value, pdMS_TO_TICKS(10)) != pdTRUE) {
    ESP_LOGW(TAG, "[PRODUCER] Queue full — sample dropped");
}
```

#### Consumer — Receiving Data

```c
// xQueueReceive(queue, pointer_to_buffer, ticks_to_wait)
// pdMS_TO_TICKS(300) = block up to 300ms waiting for data
if (xQueueReceive(sensor_queue, &received_value, pdMS_TO_TICKS(300)) == pdTRUE) {
    update_display(received_value);
} else {
    ESP_LOGW(TAG, "[CONSUMER] No data in 300ms — timeout");
}
```

The consumer blocks — it does not spin and waste CPU. If no data arrives within 300ms, it logs a timeout and loops again.

### Build & Flash

```bash
cd queue_demo

idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

### Expected Output

```
I (320) QUEUE: Boot — FreeRTOS Queue Demo
I (335) QUEUE: [PRODUCER] Sending: 1
I (535) QUEUE: [PRODUCER] Sending: 2
I (535) QUEUE: [CONSUMER] Received and displayed: 1
I (735) QUEUE: [PRODUCER] Sending: 3
I (735) QUEUE: [CONSUMER] Received and displayed: 2
```

---

## Key Concepts — Quick Reference

### `xTaskCreate()` Parameters

```c
xTaskCreate(
    task_function,   // Function pointer — the task code
    "Task_Name",     // Name — visible in debugger and task list
    2048,            // Stack size in bytes — per task, not shared
    NULL,            // pvParameters — pass a struct pointer if needed
    2,               // Priority — higher = more important, never use 0
    NULL             // TaskHandle — NULL if you won't suspend/delete it
);
```

### Stack Size Guide

| Task Type | Recommended Stack |
|---|---|
| Simple GPIO / logging | `2048` |
| String formatting (`sprintf`) | `4096` |
| HTTP / JSON parsing | `8192` |
| TLS / HTTPS | `8192–16384` |

> Stack too small → `guru meditation error: stack overflow` → increase it.

### Priority Guide

| Priority | Use for |
|---|---|
| 1 | Logging, low-urgency reporting |
| 2 | Sensor reads, periodic communication |
| 3 | Time-critical outputs (actuators, display) |
| 4–5 | ISR-deferred fast-response tasks |

> Never assign Priority 0 — reserved for the FreeRTOS IDLE task.

### `vTaskDelay()` vs `delay()`

```c
delay(500);                      // ❌ Burns CPU. Everything stalls.
vTaskDelay(pdMS_TO_TICKS(500));  // ✅ Yields CPU. Scheduler runs other tasks.
```

Always use `pdMS_TO_TICKS()` — never hardcode raw tick counts.

---

## Troubleshooting

| Problem | Cause | Fix |
|---|---|---|
| `guru meditation error: stack overflow` | Task stack too small | Increase 3rd arg in `xTaskCreate()` |
| Port not found on Linux | User not in `dialout` group | `sudo usermod -aG dialout $USER` then re-login |
| `idf.py: command not found` | ESP-IDF not exported | `source $HOME/esp/esp-idf/export.sh` |
| `IDF_PATH not set` | Environment not configured | Re-run `export.sh` |
| Build fails on first run | Target not set | Run `idf.py set-target esp32s3` before build |
| Board not detected on macOS | Driver not installed | Install CP210x or CH340 driver for your board's USB chip |
| Tasks freeze after a few seconds | `delay()` used instead of `vTaskDelay()` | Replace all `delay()` with `vTaskDelay(pdMS_TO_TICKS(...))` |
| Heap keeps dropping in STATUS log | Memory leak | Check for missing `free()` calls or queue items not being consumed |
| Queue returns NULL on create | Heap exhausted | Reduce other allocations; check for leaks with `heap_caps_get_free_size()` |

---

## Adapting for Your Board

**1. Change the target:**
```bash
idf.py set-target esp32      # original ESP32
idf.py set-target esp32s2    # ESP32-S2
idf.py set-target esp32c3    # ESP32-C3 (single core)
```

**2. Update `LED_PIN` in `main.c`:**
```c
#define LED_PIN GPIO_NUM_21   // XIAO ESP32-S3 onboard LED
#define LED_PIN GPIO_NUM_2    // Standard ESP32 DevKit onboard LED
```

---

## References

| Resource | Link |
|---|---|
| ESP-IDF Getting Started | [docs.espressif.com](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/index.html) |
| ESP-IDF FreeRTOS API | [docs.espressif.com/freertos](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/freertos.html) |
| FreeRTOS Official Docs | [freertos.org](https://www.freertos.org/Documentation/RTOS_book.html) |
| XIAO ESP32-S3 Wiki | [wiki.seeedstudio.com](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/) |
| Full Article | [analogdata.blog](https://analogdata.blog) |
| ESP-IDF Curriculum | [build.analogdata.io](https://build.analogdata.io) |
| Edge AI + IoT Workshop | [edgeai.analogdata.io](https://edgeai.analogdata.io) |

---

## License

MIT License — free to use, modify, and distribute with attribution.

---

## Author

**Rajath Kumar K S**  
Founder, [Analog Data](https://analogdata.io) — Embedded Systems, IoT, AI/ML & Edge AI  
📧 rajath@analogdata.io

[analogdata.blog](https://analogdata.blog) · [LinkedIn](https://www.linkedin.com/in/rajathkumarks) · [YouTube](https://www.youtube.com/@analogdataio) · [X](https://x.com/rajathkumarks)