# interleaved_pwm
![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.x-blue)
![Espressif Component Registry](https://img.shields.io/badge/Espressif-Component%20Registry-orange)
![License](https://img.shields.io/badge/license-MIT-green)

ESP-IDF component for **multi-phase interleaved PWM generation** using the LEDC peripheral.
Instead of switching all channels simultaneously, this component distributes switching events across the PWM period, reducing input ripple and improving power integrity.

---

# Features
- Multi-channel **phase-interleaved PWM**
- Automatic **even phase distribution**
- Configurable **dead time enforcement**
- Runtime **per-channel pulse width control**
- Automatic **LEDC timer resolution selection**
- Cross-chip support using `soc_caps`
- Validation of timing constraints at creation

---

# Installation

## Using ESP-IDF Component Manager (Recommended)
```bash
idf.py add-dependency "embedblocks/interleaved_pwm^0.2.0"
```
Or in your project's `idf_component.yml`:
```yaml
dependencies:
  embedblocks/interleaved_pwm: "^0.2.0"
```

---

# How it works

The PWM period is divided into equal **slots**, one per channel:
```
slot = period / total_channels
```
Each channel is phase-shifted into its own slot.

```
Time →   0        5000      10000     15000     20000  (µs)
         |---------|---------|---------|---------|
CH0      [  ON  ]__|
CH1               [  ON  ]__|
CH2                         [  ON  ]__|
CH3                                   [  ON  ]__|
```

---

# Dead Time Constraint
```
pulse_width + dead_time ≤ slot
```
If violated, creation fails.

---

# Frequency and Resolution
- Resolution is auto-selected based on:
  - frequency
  - number of channels
- Guarantees **≥5% granularity per slot**
- Maximum resolution: **13 bits**

---

# Chip Support

| Chip     | Status |
|----------|--------|
| ESP32    | ✅ Tested |
| ESP32-S2 | ⚠️ Expected to work |
| ESP32-S3 | ⚠️ Expected to work |
| ESP32-C3 | ⚠️ Expected to work |

The component uses `soc/soc_caps.h` and targets the LEDC low-speed mode,
making it portable across ESP-IDF supported chips.

However, only ESP32 has been fully validated so far.
---

# API

## Create
```c
esp_err_t interleavedPWMCreate(
    interleaved_pwm_config_t *config,
    interleaved_pwm_handle_t **out_handle);
```

---

## Control
```c
PWM_START(handle);
PWM_STOP(handle);
PWM_DESTROY(&handle);
PWM_SET_WIDTH(handle, channel, width);
```

---

# Usage Example
```c
#include "interleaved_pwm.h"

static uint8_t gpio_list[4] = {5, 18, 22, 23};
static uint32_t widths[4]   = {2000, 2000, 2000, 2000};

interleaved_pwm_handle_t *pwm = NULL;
interleaved_pwm_config_t config = {
    .gpio_no      = gpio_list,
    .pulse_widths = widths,
    .total_gpio   = 4,
    .dead_time    = 1000,
    .frequency    = 50
};

ESP_ERROR_CHECK(interleavedPWMCreate(&config, &pwm));
PWM_START(pwm);
PWM_SET_WIDTH(pwm, 0, 3000);
PWM_STOP(pwm);
PWM_DESTROY(&pwm);
```

---

# Limitations

![Stop Problem](https://raw.githubusercontent.com/embedblocks/interleaved-pwm/v0.2.0/docs/1st-cycle-fixed.png)

- The stop is delayed.
- Single instance per application in the current release
- Timer 0 is always used — not configurable
- 13-bit resolution cap (sufficient for all supported chips)
- No dynamic channel reallocation after creation

---
## Examples

The component includes two examples demonstrating typical usage patterns.

| Example | Description |
|---------|-------------|
| `simple` | Minimal setup of a multi-channel interleaved PWM instance. Shows basic creation, start, and steady-state operation. |
| `ramp` | Demonstrates runtime pulse width updates using a ramp pattern across channels. Useful for observing dynamic behaviour and transient response. |

---

### Running an example

1. Navigate to the example directory:

```ESP-IDF Terminal
cd examples/simple
idf.py build flash monitor


# License
MIT License