/*
 * SPDX-License-Identifier: MIT
 * Garden Door Controller – Doorbell (Optocoupler) Driver
 */

#include "doorbell_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "doorbell_drv";

static gpio_num_t         s_doorbell_gpio = DOORBELL_DEFAULT_GPIO;
static doorbell_event_cb_t s_event_cb     = NULL;
static QueueHandle_t       s_evt_queue    = NULL;

/* ---------- ISR ---------- */

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)(uintptr_t)arg;
    xQueueSendFromISR(s_evt_queue, &gpio_num, NULL);
}

/* ---------- Task that processes ISR events ---------- */

static void doorbell_task(void *arg)
{
    uint32_t io_num;
    bool last_state = false;

    for (;;) {
        if (xQueueReceive(s_evt_queue, &io_num, portMAX_DELAY)) {
            /* Simple debounce – wait 50 ms, then read the stable level */
            vTaskDelay(pdMS_TO_TICKS(50));
            bool active = (gpio_get_level(s_doorbell_gpio) == 0);  /* active-low */
            if (active != last_state) {
                last_state = active;
                ESP_LOGI(TAG, "Doorbell %s", active ? "RINGING" : "IDLE");
                if (s_event_cb) {
                    s_event_cb(active);
                }
            }
        }
    }
}

/* ---------- public API ---------- */

esp_err_t doorbell_driver_init(gpio_num_t gpio_num, doorbell_event_cb_t cb)
{
    s_doorbell_gpio = gpio_num;
    s_event_cb      = cb;

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << s_doorbell_gpio,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return err;
    }

    s_evt_queue = xQueueCreate(8, sizeof(uint32_t));
    if (s_evt_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_ERR_NO_MEM;
    }

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        /* ESP_ERR_INVALID_STATE means the ISR service is already installed */
        ESP_LOGE(TAG, "gpio_install_isr_service failed: %s", esp_err_to_name(err));
        return err;
    }
    gpio_isr_handler_add(s_doorbell_gpio, gpio_isr_handler,
                         (void *)(uintptr_t)s_doorbell_gpio);

    xTaskCreate(doorbell_task, "doorbell", 3072, NULL, 10, NULL);

    ESP_LOGI(TAG, "Doorbell driver initialised on GPIO %d", s_doorbell_gpio);
    return ESP_OK;
}

bool doorbell_driver_get_state(void)
{
    return gpio_get_level(s_doorbell_gpio) == 0;  /* active-low */
}
