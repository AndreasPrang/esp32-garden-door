/*
 * SPDX-License-Identifier: MIT
 *
 * ESP32-H2 Garden Door Controller
 *
 * Matter device with two endpoints:
 *   1. Door Lock endpoint     – controls the relay (buzzer) to open the gate.
 *   2. Contact Sensor endpoint – reports doorbell rings as contact open/close.
 *      Apple Home sends a notification when the sensor opens.
 *
 * Transport: Thread (IEEE 802.15.4) – works with Apple Home via Thread
 * border router.
 */

#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <nvs_flash.h>

#include <esp_matter.h>
#include <esp_matter_endpoint.h>

#include <app/clusters/door-lock-server/door-lock-server.h>
#include <platform/PlatformManager.h>

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#include <Esp32ThreadInit.h>
#endif

#include "drivers/relay_driver.h"
#include "drivers/doorbell_driver.h"

static const char *TAG = "garden_door";

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

/* ------------------------------------------------------------------ */
/*  Endpoint / cluster IDs (filled during create)                     */
/* ------------------------------------------------------------------ */
static uint16_t s_lock_endpoint_id    = 0;
static uint16_t s_doorbell_endpoint_id = 0;

/* Duration the relay stays on when "unlock" is requested (ms) */
#define DOOR_OPEN_PULSE_MS  CONFIG_GARDEN_DOOR_RELAY_PULSE_MS

/* Auto-lock delay after unlock (seconds) – configured via Kconfig */
#define AUTO_LOCK_DELAY_S   CONFIG_GARDEN_DOOR_AUTO_LOCK_DELAY_S

static esp_timer_handle_t s_auto_lock_timer = NULL;
static esp_timer_handle_t s_doorbell_reset_timer = NULL;

static void auto_lock_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "Auto-lock: locking door after %d s", AUTO_LOCK_DELAY_S);
    relay_driver_set(true);

    /* Must hold CHIP stack lock when modifying attributes */
    using namespace chip::app::Clusters::DoorLock;
    chip::DeviceLayer::PlatformMgr().LockChipStack();
    DoorLockServer::Instance().SetLockState(s_lock_endpoint_id,
        DlLockState::kLocked, OperationSourceEnum::kAuto);
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
}

/* Reset contact sensor back to "no contact" (false) after short pulse */
static void doorbell_reset_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "Doorbell: resetting contact sensor to closed");
    esp_matter_attr_val_t val = esp_matter_nullable_bool(false);
    attribute::update(s_doorbell_endpoint_id,
                      BooleanState::Id,
                      BooleanState::Attributes::StateValue::Id,
                      &val);
}

/* ------------------------------------------------------------------ */
/*  Forward declarations                                              */
/* ------------------------------------------------------------------ */
static void doorbell_event_handler(bool ringing);

/* ------------------------------------------------------------------ */
/*  Matter event callback (commissioning, fabric events, etc.)        */
/* ------------------------------------------------------------------ */
static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;
    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(TAG, "Commissioning session stopped");
        break;
    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        ESP_LOGI(TAG, "Fabric removed");
        break;
    case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized:
        ESP_LOGI(TAG, "BLE deinitialized and memory reclaimed");
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/*  Matter attribute-update callback                                  */
/* ------------------------------------------------------------------ */
static esp_err_t app_attribute_update_cb(
    attribute::callback_type_t type,
    uint16_t endpoint_id,
    uint32_t cluster_id,
    uint32_t attribute_id,
    esp_matter_attr_val_t *val,
    void *priv_data)
{
    if (type == PRE_UPDATE) {
        /* Door-lock endpoint: LockState attribute */
        if (endpoint_id == s_lock_endpoint_id &&
            cluster_id  == DoorLock::Id &&
            attribute_id == DoorLock::Attributes::LockState::Id) {

            if (val == NULL) {
                return ESP_OK;
            }

            /* Matter DoorLock LockState: 1 = Locked, 2 = Unlocked */
            uint8_t lock_state = val->val.u8;
            ESP_LOGI(TAG, "LockState update -> %d", lock_state);

            if (lock_state == 2) {
                /* Unlocked -> deactivate relay and schedule auto-lock */
                relay_driver_set(false);
                if (s_auto_lock_timer) {
                    esp_timer_stop(s_auto_lock_timer); /* reset if already running */
                    esp_timer_start_once(s_auto_lock_timer,
                                         (uint64_t)AUTO_LOCK_DELAY_S * 1000000ULL);
                }
                ESP_LOGI(TAG, "Door unlocked – auto-lock in %d s", AUTO_LOCK_DELAY_S);
            } else {
                /* Locked -> cancel timer, ensure relay is on */
                if (s_auto_lock_timer) {
                    esp_timer_stop(s_auto_lock_timer);
                }
                relay_driver_set(true);
            }
        }
    }
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  Matter identification callback (e.g. blink an LED)                */
/* ------------------------------------------------------------------ */
static esp_err_t app_identification_cb(
    identification::callback_type_t type,
    uint16_t endpoint_id,
    uint8_t effect_id,
    uint8_t effect_variant,
    void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: endpoint %d, effect %d", endpoint_id, effect_id);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  Doorbell → Matter Contact Sensor (open / close pulse)             */
/* ------------------------------------------------------------------ */
static void doorbell_event_handler(bool ringing)
{
    if (!ringing) {
        return; /* only react to press, not release */
    }
    ESP_LOGI(TAG, "Doorbell RING – setting contact sensor to OPEN");

    /* Set contact sensor to "open" (true = contact detected / ring) */
    esp_matter_attr_val_t val = esp_matter_nullable_bool(true);
    attribute::update(s_doorbell_endpoint_id,
                      BooleanState::Id,
                      BooleanState::Attributes::StateValue::Id,
                      &val);

    /* Reset back to closed after 1 second */
    esp_timer_stop(s_doorbell_reset_timer);
    esp_timer_start_once(s_doorbell_reset_timer, 1000000ULL);
}

/* ------------------------------------------------------------------ */
/*  app_main                                                          */
/* ------------------------------------------------------------------ */
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "========= Garden Door Controller =========");

    /* --- NVS --- */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* --- Hardware drivers --- */
    ESP_ERROR_CHECK(relay_driver_init(RELAY_DEFAULT_GPIO));
    ESP_ERROR_CHECK(doorbell_driver_init(DOORBELL_DEFAULT_GPIO,
                                          doorbell_event_handler));

    /* --- Auto-lock timer --- */
    const esp_timer_create_args_t auto_lock_args = {
        .callback = auto_lock_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "auto_lock",
    };
    ESP_ERROR_CHECK(esp_timer_create(&auto_lock_args, &s_auto_lock_timer));

    /* --- Doorbell reset timer --- */
    const esp_timer_create_args_t doorbell_reset_args = {
        .callback = doorbell_reset_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "doorbell_rst",
    };
    ESP_ERROR_CHECK(esp_timer_create(&doorbell_reset_args, &s_doorbell_reset_timer));

    /* --- Matter node --- */
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb,
                                 app_identification_cb);
    if (node == NULL) {
        ESP_LOGE(TAG, "Failed to create Matter root node");
        return;
    }

    /* --- Endpoint 1: Door Lock ----------------------------------- */
    {
        door_lock::config_t lock_config;
        lock_config.door_lock.lock_state = nullable<uint8_t>(1); /* Locked */
        lock_config.door_lock.lock_type  = 0;                    /* Dead bolt */
        lock_config.door_lock.actuator_enabled = true;
        endpoint_t *ep = door_lock::create(node, &lock_config,
                                            ENDPOINT_FLAG_NONE, NULL);
        if (ep == NULL) {
            ESP_LOGE(TAG, "Failed to create door_lock endpoint");
            return;
        }
        s_lock_endpoint_id = endpoint::get_id(ep);
        ESP_LOGI(TAG, "Door Lock endpoint created: %d", s_lock_endpoint_id);
    }

    /* --- Endpoint 2: Contact Sensor (doorbell) -------------------- */
    {
        contact_sensor::config_t cs_config;
        cs_config.boolean_state.state_value = false; /* closed = no ring */
        endpoint_t *ep = contact_sensor::create(node, &cs_config,
                                                 ENDPOINT_FLAG_NONE, NULL);
        if (ep == NULL) {
            ESP_LOGE(TAG, "Failed to create contact_sensor endpoint");
            return;
        }
        s_doorbell_endpoint_id = endpoint::get_id(ep);
        ESP_LOGI(TAG, "Contact Sensor (doorbell) endpoint created: %d",
                 s_doorbell_endpoint_id);
    }

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    /* Set OpenThread platform config */
    esp_openthread_platform_config_t ot_config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config  = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config  = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    set_openthread_platform_config(&ot_config);
#endif

    /* Start the Matter stack (includes Thread networking) */
    err = esp_matter::start(app_event_cb);
    ESP_ERROR_CHECK(err);

    /* Force door locked state on every boot for safety */
    {
        using namespace chip::app::Clusters::DoorLock;
        chip::DeviceLayer::PlatformMgr().LockChipStack();
        DoorLockServer::Instance().SetLockState(s_lock_endpoint_id, DlLockState::kLocked);
        chip::DeviceLayer::PlatformMgr().UnlockChipStack();
        relay_driver_set(true);
        ESP_LOGI(TAG, "Boot: door forced to LOCKED state");
    }

    ESP_LOGI(TAG, "Matter stack started – waiting for commissioning …");
}
