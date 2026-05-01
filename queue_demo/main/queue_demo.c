#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "QUEUE_DEMO";
QueueHandle_t sensor_queue;

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

// ── Producer: reads sensor, pushes to queue ────────────────────────────
void sensor_producer_task(void *pvParameters) {
    int value;
    while (1) {
        value = read_sensor();
        ESP_LOGI(TAG, "[PRODUCER] Sending: %d", value);

        if (xQueueSend(sensor_queue, &value, pdMS_TO_TICKS(10)) != pdTRUE) {
            ESP_LOGW(TAG, "[PRODUCER] Queue full — sample dropped");
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// ── Consumer: pulls from queue, updates display ────────────────────────
void display_consumer_task(void *pvParameters) {
    int received_value;
    while (1) {
        if (xQueueReceive(sensor_queue, &received_value, pdMS_TO_TICKS(300)) == pdTRUE) {
            update_display(received_value);
        } else {
            ESP_LOGW(TAG, "[CONSUMER] No data in 300ms — timeout");
        }
    }
}

// ── Entry point ────────────────────────────────────────────────────────
void app_main(void) {
    ESP_LOGI(TAG, "Boot — FreeRTOS Queue Demo");

    sensor_queue = xQueueCreate(10, sizeof(int));

    if (sensor_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue — halting");
        return;
    }

    xTaskCreate(sensor_producer_task, "Producer", 2048, NULL, 2, NULL);
    xTaskCreate(display_consumer_task, "Consumer", 4096, NULL, 1, NULL);
}