# interleaved_pwm

An ESP-IDF component for multi-phase interleaved PWM generation using the LEDC peripheral. Distributes evenly across the period.

---

## How it works

Instead of switching all channels simultaneously, each channel is shifted in phase so that only one channel switches at a time. The full period is divided into equal slots — one per channel:

```
slot = time_period / total_channels
```

```
Time →   0        5000      10000     15000     20000  (µs)
         |---------|---------|---------|---------|
CH0      [  ON  ]__|
CH1               [  ON  ]__|
CH2                         [  ON  ]__|
CH3                                   [  ON  ]__|
```

This distributes switching stress evenly and reduces the ripple seen by the input supply compared to synchronous switching.

---

## Dead time

`dead_time` is a mandatory blanking gap enforced after each pulse before the next channel is allowed to turn on. It prevents simultaneous conduction across channels.

The component validates this constraint on every channel at creation time:

```
pulse_width + dead_time <= slot
```

If any channel violates this, `interleavedPWMCreate` returns an error and no instance is created.

**Example — 4 channels, period 20 000 µs:**

```
slot      = 20000 / 4 = 5000 µs
dead_time = 1000 µs
max valid pulse_width = 5000 - 1000 = 4000 µs

CH0: 2000 + 1000 = 3000 ≤ 5000  ✓
CH1: 3000 + 1000 = 4000 ≤ 5000  ✓
CH2: 1500 + 1000 = 2500 ≤ 5000  ✓
CH3: 4000 + 1000 = 5000 ≤ 5000  ✓  (boundary)
```

---

## Frequency and resolution

The component automatically computes the optimal LEDC timer resolution from the requested frequency and channel count. The resolution is selected to guarantee at least **5% granularity per channel slot**.

Timer resolution is capped at **13 bits**, which is the maximum supported across all target chips.

If the requested frequency is too high to satisfy the granularity requirement for the given number of channels, `interleavedPWMCreate` returns an error.

**Maximum supported frequencies by channel count\*:**

| Channels | Max frequency |
|----------|--------------|
| 1        | 2.5 MHz      |
| 2        | 1.25 MHz     |
| 4        | 625 kHz      |
| 6        | 416 kHz      |
| 8        | 312 kHz      |

\* *At 80 MHz APB clock with 5% slot granularity threshold and 13-bit resolution cap. Actual maximum may be lower depending on target chip channel availability and system clock configuration.*

---

## Chip support

| Chip     | Supported |
|----------|-----------|
| ESP32    | ✅        |
| ESP32-S2 | ✅        |
| ESP32-S3 | ✅        |
| ESP32-C3 | ✅        |
| ESP32-H2 | ✅        |

The component detects the LEDC speed mode and maximum channel count at compile time using `soc/soc_caps.h`. No manual configuration is required when switching targets.

> **Note:** The current release supports a **single instance**. The number of channels per instance is limited to the number of LEDC channels available on the target chip (`SOC_LEDC_CHANNEL_NUM`).

---

## Installation

Clone or copy the component into your project's `components` directory:

```
your_project/
├── components/
│   └── interleaved_pwm/
│       ├── CMakeLists.txt
│       ├── interleaved_pwm.c
│       └── interleaved_pwm.h
├── main/
│   └── main.c
└── CMakeLists.txt
```

Add it to your `main/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES interleaved_pwm
)
```

---

## API

### Create

```c
int interleavedPWMCreate(interleaved_pwm_t *self, interleaved_pwm_config_t *config);
```

Initialises the instance. Returns `0` on success, non-zero on any validation or peripheral error.

**Config fields:**

| Field          | Type       | Description                                      |
|----------------|------------|--------------------------------------------------|
| `gpio_no`      | `uint8_t*` | Array of GPIO numbers, one per channel           |
| `pulse_widths` | `uint32_t*`| Initial pulse width per channel in microseconds  |
| `total_gpio`   | `uint8_t`  | Number of channels (must not exceed chip maximum)|
| `dead_time`    | `uint32_t` | Blanking gap in microseconds                     |
| `frequency`    | `uint32_t` | Switching frequency in Hz                        |

### Control macros

All macros accept a **pointer** to the instance. Pass `&pwm` if you hold the instance by value.

```c
PWM_START(&pwm);                    // Start all channels
PWM_STOP(&pwm);                     // Stop all channels, outputs go idle
PWM_DESTROY(&pwm);                  // Stop, release peripheral, reset GPIOs
PWM_SET_WIDTH(&pwm, channel, width);// Change pulse width of one channel at runtime
```

`PWM_SET_WIDTH` applies the same slot constraint as creation. A rejected width leaves the channel unchanged.

---

## Usage

```c
#include "interleaved_pwm.h"

static uint8_t  gpio_list[4]    = {5, 18, 22, 23};
static uint32_t pulse_widths[4] = {2000, 2000, 2000, 2000};

interleaved_pwm_t pwm;

interleaved_pwm_config_t config = {
    .gpio_no      = gpio_list,
    .pulse_widths = pulse_widths,
    .total_gpio   = 4,
    .dead_time    = 1000,
    .frequency    = 50
};

// Create
int ret = interleavedPWMCreate(&pwm, &config);
if (ret != 0) { /* handle error */ }

// Start
PWM_START(&pwm);

// Adjust duty at runtime
PWM_SET_WIDTH(&pwm, 0, 3000);
PWM_SET_WIDTH(&pwm, 1, 1500);

// Stop and release
PWM_STOP(&pwm);
PWM_DESTROY(&pwm);
```

---

## Examples

| Example | Description |
|---------|-------------|
| `interleaved_pwm_getting_started` | Single 4-channel instance. Explains slot and dead time constraints with worked calculations in the serial output. |
| `interleaved_pwm_dual_converter_ramp` | Two independent instances running simultaneously with a duty ramp, demonstrating runtime width changes and full power-saving shutdown. |

---

![Start Problem](https://raw.githubusercontent.com/khiyamiftikhar/v0.1.5-ledc/ledc-publish/docs/1st-cycle.png)

## Limitations

- All channels may not be correct on 1-2 cycle after start because of LEDC limitation
- Single instance per application in the current release
- Timer 0 is always used — not configurable
- 13-bit resolution cap (sufficient for all supported chips)
- No dynamic channel reallocation after creation

