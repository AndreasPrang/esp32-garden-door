# ESP32-H2 Garden Door Controller

A **Matter-over-Thread** garden gate controller running on the
**ESP32-H2-DevKitM-1**. It lets you unlock your garden gate and receive
doorbell notifications straight from the **Apple Home** app on your iPhone.

## Features

| Feature | Matter Device Type | Hardware |
|---|---|---|
| **Gate opener** (buzzer relay) | Door Lock | Relay module (active-high) |
| **Doorbell detection** | Generic Switch (Momentary) | Optocoupler (active-low) |

* Communicates over **Thread** (IEEE 802.15.4) – needs a Thread Border Router
  (e.g. Apple TV 4K, HomePod Mini).
* Fully controlled via **Apple Home** / any Matter-compatible controller.
* Door automatically locks again after a configurable delay (default 10 s).
* Door is always forced to **locked** state on every boot for safety.

## Hardware Wiring

| Signal | Default GPIO | Direction | Notes |
|---|---|---|---|
| Relay (buzzer) | `GPIO 2` | Output | Active-high; drives relay module |
| Optocoupler (doorbell) | `GPIO 3` | Input | Active-low; internal pull-up enabled |

> **Tip:** You can change the GPIOs in
> `main/drivers/relay_driver.h` and `main/drivers/doorbell_driver.h`.

### Wiring Diagram

```
                         ESP32-H2-DevKitM-1
                        ┌───────────────────┐
                    USB │ ┌───────────────┐  │
                   port │ │  ESP32-H2     │  │
                        │ │  module       │  │
                        │ └───────────────┘  │
                        │                    │
              3V3  ○────┤ 3V3          GND ├────○  GND
                        │                    │
  Relay ←── ○────┤ GPIO 2       GPIO 3 ├────○ ──→ Doorbell
                        │                    │
                        │      ...           │
                        └───────────────────┘
```

#### Relay Output (GPIO 2) – Gate Opener

```
  ESP32-H2                  Relay Module              Gate Buzzer
  ┌──────┐                 ┌───────────┐             ┌──────────┐
  │      │                 │           │             │          │
  │ GPIO2├────────────────→│ IN    COM ├─────────────┤ Terminal │
  │      │                 │        NO ├─────────────┤ Terminal │
  │  GND ├─────────────────┤ GND      │             │          │
  │  3V3 ├─────────────────┤ VCC      │             └──────────┘
  │      │                 │           │
  └──────┘                 └───────────┘

  GPIO 2 = HIGH  →  relay closes  →  buzzer activates (door opens)
  GPIO 2 = LOW   →  relay opens   →  buzzer off (door locked)
```

**Multimeter test (without relay module):**
1. Set multimeter to **DC Voltage** mode.
2. Connect **black probe** to any **GND** pin on the DevKit.
3. Connect **red probe** to **GPIO 2**.
4. Unlock the door from Apple Home.
5. You should see **≈ 3.3 V** for 10 seconds (auto-lock delay), then back to **0 V**.

#### Doorbell Input (GPIO 3) – Optocoupler

```
  Doorbell                 Optocoupler               ESP32-H2
  Switch                   (e.g. PC817)             ┌──────┐
  ┌─────┐                 ┌───────────┐             │      │
  │  ~  ├────── R1 ──────→│ Anode  Col├─────────────┤GPIO 3│
  │  ~  ├────────────────→│ Cathode Em├──────┐      │      │
  └─────┘                 └───────────┘      │      │      │
  (AC from                                   ├──────┤ GND  │
   intercom)                                 │      │      │
                                             │      │  3V3 │ (internal
                                             │      │  ┊   │  pull-up)
                                             │      │  ┊───│──→ GPIO3
                                             │      └──────┘     idle=HIGH
                                             │
                                            GND

  Idle:         GPIO 3 = HIGH  (pull-up, optocoupler OFF)
  Ring pressed: GPIO 3 = LOW   (optocoupler ON, pulls to GND)
                → triggers InitialPress event → Apple Home notification
```

**Multimeter test (without optocoupler):**
1. Set multimeter to **DC Voltage** mode.
2. Connect **black probe** to **GND** on the DevKit.
3. Connect **red probe** to **GPIO 3**.
4. Idle state: you should see **≈ 3.3 V** (internal pull-up).
5. Briefly connect **GPIO 3 to GND** with a jumper wire — this simulates a
   doorbell ring. The voltage drops to **0 V** and the serial monitor shows
   `Doorbell RING – sending InitialPress event`.

### Quick Test Without External Hardware

You only need the DevKit and a **jumper wire** (or a piece of wire):

| Test | How | Expected Result |
|------|-----|-----------------|
| **Unlock** | Send unlock from Apple Home | GPIO 2 goes HIGH (3.3 V) for 10 s, then LOW |
| **Doorbell** | Touch a wire from GPIO 3 to GND | Monitor shows `InitialPress`, Apple Home notification |

## Prerequisites

1. **ESP-IDF** v5.3 or later – <https://docs.espressif.com/projects/esp-idf/en/stable/esp32h2/get-started/>
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
   * A **Doorbell** tile (Generic Switch) – sends a notification when
     someone rings the doorbell.

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
