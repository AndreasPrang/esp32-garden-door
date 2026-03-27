# ESP32-H2 Garden Door Controller

A **Matter-over-Thread** garden gate controller running on the
**ESP32-H2-DevKitM-1**. It lets you unlock your garden gate and receive
doorbell notifications straight from the **Apple Home** app on your iPhone.

## Features

| Feature | Matter Device Type | Hardware |
|---|---|---|
| **Gate opener** (buzzer relay) | Door Lock | Relay module (active-high) |
| **Doorbell detection** | Contact Sensor | Optocoupler (active-low) |

* Communicates over **Thread** (IEEE 802.15.4) – needs a Thread Border Router
  (e.g. Apple TV 4K, HomePod Mini).
* Fully controlled via **Apple Home** / any Matter-compatible controller.
* Relay pulses for 3 seconds on "unlock" and returns to locked state.

## Hardware Wiring

| Signal | Default GPIO | Direction | Notes |
|---|---|---|---|
| Relay (buzzer) | `GPIO 2` | Output | Active-high; drives relay module |
| Optocoupler (doorbell) | `GPIO 3` | Input | Active-low; internal pull-up enabled |

> **Tip:** You can change the GPIOs in
> `main/drivers/relay_driver.h` and `main/drivers/doorbell_driver.h`.

## Prerequisites

1. **ESP-IDF** v5.1 or later – <https://docs.espressif.com/projects/esp-idf/en/stable/esp32h2/get-started/>
2. **esp-matter** SDK – <https://github.com/espressif/esp-matter>
3. Set environment variables:
   ```bash
   export IDF_PATH=/path/to/esp-idf
   export ESP_MATTER_PATH=/path/to/esp-matter
   source $IDF_PATH/export.sh
   source $ESP_MATTER_PATH/export.sh
   ```

## Build & Flash

```bash
# Set target (only needed once)
idf.py set-target esp32h2

# Build
idf.py build

# Flash via USB (adjust port if needed)
idf.py -p /dev/cu.usbmodem* flash monitor
```

## Commissioning with Apple Home

1. After flashing, the device enters **commissioning mode** automatically.
2. Open the **Home** app on your iPhone.
3. Tap **+** → **Add Accessory** → **More options…**
4. Choose **Enter Code Manually** and type: **`34970112332`**
   - Passcode: `20202021` / Discriminator: `3840` (esp-matter defaults)
5. A **Thread Border Router** (Apple TV 4K or HomePod mini) is required in your
   network to complete commissioning.
6. Once paired you will see:
   * A **Door Lock** tile – tap to unlock (the gate automatically locks
     again after 10 seconds).
   * A **Contact Sensor** tile – shows "Open" when someone rings the doorbell.

### Resetting Commissioning Data

If you need to re-commission, erase NVS:

```bash
idf.py -p /dev/cu.usbmodem* erase-flash
idf.py -p /dev/cu.usbmodem* flash monitor
```

## Configuration

The following settings can be changed via `idf.py menuconfig` under
**Garden Door Controller**:

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_GARDEN_DOOR_AUTO_LOCK_DELAY_S` | **10** | Seconds before the door automatically locks after an unlock command (1–300) |
| `CONFIG_GARDEN_DOOR_RELAY_PULSE_MS` | **3000** | Duration the relay stays energised (ms) – the buzzer activation time (500–30000) |

After changing values, rebuild and re-flash:

```bash
idf.py menuconfig   # change settings
idf.py build flash monitor
```

## Project Structure

```
esp32-garden-door/
├── CMakeLists.txt              # Top-level project CMake
├── partitions.csv              # Custom partition table
├── sdkconfig.defaults          # Default Kconfig for ESP32-H2 + Matter
├── README.md
└── main/
    ├── CMakeLists.txt          # Component CMake
    ├── Kconfig.projbuild       # Configurable options (auto-lock delay, relay pulse)
    ├── idf_component.yml       # esp-matter dependency
    ├── app_main.cpp            # Application entry – Matter endpoints
    └── drivers/
        ├── relay_driver.h      # Relay (buzzer) driver API
        ├── relay_driver.c      # Relay implementation
        ├── doorbell_driver.h   # Doorbell (optocoupler) driver API
        ├── doorbell_driver.c   # Doorbell implementation
        └── door_lock_callbacks.cpp  # Matter Door Lock cluster callbacks
```

## License

MIT
