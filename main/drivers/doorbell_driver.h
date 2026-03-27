/*
 * SPDX-License-Identifier: MIT
 * Garden Door Controller – Doorbell (Optocoupler) Driver
 *
 * Monitors the optocoupler output to detect doorbell ring events.
 * When the input goes active a callback is invoked.
 */

#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Default GPIO – choose any free pin on the ESP32-H2-DevKitM-1 */
#define DOORBELL_DEFAULT_GPIO  GPIO_NUM_3

/** Callback type invoked when a doorbell event is detected. */
typedef void (*doorbell_event_cb_t)(bool pressed);

/**
 * @brief Initialise the doorbell input GPIO (active-low, internal pull-up).
 *
 * The optocoupler pulls the line LOW when the doorbell rings.
 *
 * @param gpio_num  GPIO number connected to the optocoupler output.
 * @param cb        Callback for ring events (called from ISR context via deferred work).
 * @return ESP_OK on success.
 */
esp_err_t doorbell_driver_init(gpio_num_t gpio_num, doorbell_event_cb_t cb);

/**
 * @brief Read current doorbell state.
 *
 * @return true if the doorbell line is active (ringing).
 */
bool doorbell_driver_get_state(void);

#ifdef __cplusplus
}
#endif
