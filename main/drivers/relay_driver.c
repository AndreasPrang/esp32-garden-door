/*
 * SPDX-License-Identifier: MIT
 * Garden Door Controller – Relay Driver
 */

#include "relay_driver.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "relay_drv";

static gpio_num_t s_relay_gpio = RELAY_DEFAULT_GPIO;
static bool       s_relay_state = false;
static esp_timer_handle_t s_pulse_timer = NULL;

/* ---------- pulse timer callback ---------- */

static void pulse_timer_cb(void *arg)
{
    relay_driver_set(false);
    ESP_LOGI(TAG, "Pulse finished – relay OFF");
}

/* ---------- public API ---------- */

esp_err_t relay_driver_init(gpio_num_t gpio_num)
{
    s_relay_gpio = gpio_num;

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << s_relay_gpio,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return err;
    }
    gpio_set_level(s_relay_gpio, 0);
    s_relay_state = false;

    /* Create one-shot timer for pulse */
    const esp_timer_create_args_t timer_args = {
        .callback = pulse_timer_cb,
        .name     = "relay_pulse",
    };
    err = esp_timer_create(&timer_args, &s_pulse_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_create failed: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Relay driver initialised on GPIO %d", s_relay_gpio);
    return ESP_OK;
}

void relay_driver_set(bool on)
{
    gpio_set_level(s_relay_gpio, on ? 1 : 0);
    s_relay_state = on;
    ESP_LOGI(TAG, "Relay %s", on ? "ON" : "OFF");
}

void relay_driver_pulse(uint32_t duration_ms)
{
    if (s_pulse_timer == NULL) {
        ESP_LOGE(TAG, "Pulse timer not initialised");
        return;
    }
    /* Stop any running pulse first */
    esp_timer_stop(s_pulse_timer);

    relay_driver_set(true);
    esp_timer_start_once(s_pulse_timer, (uint64_t)duration_ms * 1000);
    ESP_LOGI(TAG, "Relay pulsed for %lu ms", (unsigned long)duration_ms);
}

bool relay_driver_get_state(void)
{
    return s_relay_state;
}
