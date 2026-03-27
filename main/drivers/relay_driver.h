/*
 * SPDX-License-Identifier: MIT
 * Garden Door Controller – Relay Driver
 *
 * Drives the relay that activates the door buzzer.
 */

#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Default GPIO – choose any free pin on the ESP32-H2-DevKitM-1 */
#define RELAY_DEFAULT_GPIO  GPIO_NUM_2

/**
 * @brief Initialise the relay GPIO as push-pull output (active-high).
 *
 * @param gpio_num  GPIO number connected to the relay module.
 * @return ESP_OK on success.
 */
esp_err_t relay_driver_init(gpio_num_t gpio_num);

/**
 * @brief Set the relay state.
 *
 * @param on  true = buzzer on (door open), false = buzzer off.
 */
void relay_driver_set(bool on);

/**
 * @brief Pulse the relay for a given duration (non-blocking, uses esp_timer).
 *
 * After @p duration_ms the relay is turned off automatically.
 *
 * @param duration_ms  Pulse length in milliseconds.
 */
void relay_driver_pulse(uint32_t duration_ms);

/**
 * @brief Return the current relay state.
 */
bool relay_driver_get_state(void);

#ifdef __cplusplus
}
#endif
