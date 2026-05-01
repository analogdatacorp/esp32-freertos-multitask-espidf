// =============================================================================
// Project  : esp32-freertos-multitask-espidf
// File     : multitask_demo/main/main.c
// Author   : Rajath Kumar K S — analogdata.io
// Brief    : FreeRTOS multi-task demo on ESP32-S3 using ESP-IDF v5.x
//            Three independent tasks running simultaneously:
//              Task 1 — LED blink every 500ms
//              Task 2 — Simulated sensor read every 200ms
//              Task 3 — System status log every 1 second
// Board    : Seeed Studio XIAO ESP32-S3 (onboard LED on GPIO 21)
// IDF Ver  : v6.x
// Blog     : https://analogdata.blog
// =============================================================================

#include <stdio.h>
#include "freertos/FreeRTOS.h"   // Core FreeRTOS definitions — must always be first
#include "freertos/task.h"       // xTaskCreate(), vTaskDelay(), TaskHandle_t
#include "driver/gpio.h"         // gpio_config(), gpio_set_level()
#include "esp_log.h"             // ESP_LOGI(), ESP_LOGW(), ESP_LOGE()
#include "esp_system.h"          // esp_get_free_heap_size()

// =============================================================================
// CONFIGURATION
// =============================================================================

// GPIO pin for the onboard LED on XIAO ESP32-S3
// Change this to GPIO_NUM_2 for standard ESP32 DevKit boards
#define LED_PIN GPIO_NUM_21

// Log tag — appears as the component name in the serial monitor output
// e.g. "I (335) MULTITASK: [SENSOR] Value: 1"
static const char *TAG = "MULTITASK";

// =============================================================================
// TASK 1 — LED BLINK
// Toggles the onboard LED ON/OFF every 500ms independently of all other tasks.
//
// xTaskCreate() params used:
//   Stack : 2048 bytes — sufficient for simple GPIO operations
//   Priority : 3 — highest of the three tasks; LED timing should be tight
// =============================================================================
void led_blink_task(void *pvParameters) {

    // Configure GPIO 21 as a push-pull output
    // gpio_config_t is a struct — set only what you need, rest defaults to 0
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),  // Bitmask: select GPIO 21
        .mode         = GPIO_MODE_OUTPUT,   // Set as output
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,  // No interrupt needed
    };
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "LED Blink Task started — Core %d", xPortGetCoreID());

    while (1) {
        gpio_set_level(LED_PIN, 1);             // LED ON
        vTaskDelay(pdMS_TO_TICKS(500));         // Yield CPU for 500ms — other tasks run during this time

        gpio_set_level(LED_PIN, 0);             // LED OFF
        vTaskDelay(pdMS_TO_TICKS(500));         // Yield CPU for 500ms again

        // Total cycle: 1000ms = 1Hz blink rate
        // vTaskDelay() is NOT the same as delay() —
        // it yields the CPU to the scheduler instead of burning cycles
    }
}

// =============================================================================
// TASK 2 — SENSOR READ (SIMULATED)
// Reads a sensor value every 200ms and logs it to UART.
//
// Currently simulated with a counter (0–99, wraps around).
// To use a real sensor — replace the simulation block with your
// actual ADC read, I2C read, or SPI transaction.
//
// xTaskCreate() params used:
//   Stack : 2048 bytes — sufficient for logging and simple arithmetic
//   Priority : 2 — mid priority; sensor reads are important but not critical
// =============================================================================
void sensor_read_task(void *pvParameters) {

    // sensor_value is a local variable — stored on THIS task's stack
    // Each FreeRTOS task has its own stack, so no conflict with other tasks
    int sensor_value = 0;

    ESP_LOGI(TAG, "Sensor Read Task started — Core %d", xPortGetCoreID());

    while (1) {
        // ── SIMULATED SENSOR READ ─────────────────────────────────────────
        // Replace this block with your real sensor read, for example:
        //   int sensor_value = adc1_get_raw(ADC1_CHANNEL_0);        // ADC
        //   i2c_master_read_slave(i2c_num, data, len);              // I2C
        //   spi_device_transmit(spi_handle, &transaction);          // SPI
        sensor_value = (sensor_value + 1) % 100;
        // ─────────────────────────────────────────────────────────────────

        // Log the value — visible in idf.py monitor
        ESP_LOGI(TAG, "[SENSOR] Value: %d°C (simulated)", sensor_value);

        // Yield CPU for 200ms — LED task and UART task can run during this time
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// =============================================================================
// TASK 3 — UART STATUS LOG
// Logs system health (free heap + uptime) every 1 second.
// Useful for detecting memory leaks (free heap shrinking over time = leak).
//
// xTaskCreate() params used:
//   Stack : 4096 bytes — larger because ESP_LOGI with multiple format args
//           uses more stack than simple GPIO operations
//   Priority : 1 — lowest priority; logging is not time-critical
// =============================================================================
void uart_log_task(void *pvParameters) {

    int log_count = 0;

    ESP_LOGI(TAG, "UART Log Task started — Core %d", xPortGetCoreID());

    while (1) {
        // esp_get_free_heap_size() returns current free internal heap in bytes
        // If this number keeps dropping over time → you have a memory leak
        ESP_LOGI(TAG,
                 "[STATUS] Log #%d | Heap Free: %lu bytes | Uptime: %lu ms",
                 log_count++,
                 (unsigned long)esp_get_free_heap_size(),
                 (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS));

        // Yield CPU for 1 second
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// =============================================================================
// APP_MAIN — ENTRY POINT
// In ESP-IDF, app_main() replaces main(). It runs as a FreeRTOS task itself
// at a default priority. Once all tasks are created and app_main() returns,
// the FreeRTOS scheduler takes full control and runs your tasks indefinitely.
// =============================================================================
void app_main(void) {

    ESP_LOGI(TAG, "=== FreeRTOS Multi-Task Demo ===");
    ESP_LOGI(TAG, "Chip: ESP32-S3 | Cores: 2 | FreeRTOS Tick: %d Hz",
             configTICK_RATE_HZ);

    // ── CREATE TASK 1: LED Blink ──────────────────────────────────────────
    // xTaskCreate(function, name, stack_bytes, params, priority, handle)
    //   function    : led_blink_task — the task's entry function
    //   name        : "LED_Blink"   — shown in debugger / task list
    //   stack_bytes : 2048          — per-task stack, not shared
    //   params      : NULL          — no parameters passed
    //   priority    : 3             — highest of the three tasks
    //   handle      : NULL          — not needed; we won't suspend/delete it
    xTaskCreate(led_blink_task, "LED_Blink", 2048, NULL, 3, NULL);

    // ── CREATE TASK 2: Sensor Read ────────────────────────────────────────
    xTaskCreate(sensor_read_task, "Sensor_Read", 2048, NULL, 2, NULL);

    // ── CREATE TASK 3: UART Log ───────────────────────────────────────────
    // Stack is 4096 (not 2048) because ESP_LOGI with multiple format
    // specifiers (%d, %lu, %lu) uses more stack than a simple GPIO write
    xTaskCreate(uart_log_task, "UART_Log", 4096, NULL, 1, NULL);

    ESP_LOGI(TAG, "All 3 tasks created — FreeRTOS scheduler now running");

    // app_main() returns here — FreeRTOS scheduler takes over
    // The three tasks above will now run indefinitely, each on their
    // own schedule, yielding to each other via vTaskDelay()
}