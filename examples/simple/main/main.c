/*
 * main.c — interleaved_pwm_getting_started
 *
 * This example explains the core idea of interleaved PWM and walks
 * through how the configuration fields relate to each other before
 * creating a single 4-channel instance and running it.
 *
 * ---------------------------------------------------------------
 *  The idea of interleaved PWM
 * ---------------------------------------------------------------
 *
 *  Instead of switching all channels at the same time, each channel
 *  is shifted in phase so that only one channel switches at a time.
 *  For N channels the full period is divided into N equal slots:
 *
 *    slot = time_period / total_gpio
 *
 *  Example with 4 channels and time_period = 20 000 µs:
 *
 *    slot = 20 000 / 4 = 5 000 µs per channel
 *
 *  Each channel gets exactly one slot to do its work:
 *
 *    Time →   0        5000      10000     15000     20000
 *             |---------|---------|---------|---------|
 *    CH0      [  ON  ]__|                            |
 *    CH1               [  ON  ]__|                  |
 *    CH2                         [  ON  ]__|        |
 *    CH3                                   [  ON  ]_|
 *
 *  This spreads the switching events evenly across the period,
 *  reducing input current ripple compared to all channels switching
 *  together — a key benefit in multi-phase power converters.
 *
 * ---------------------------------------------------------------
 *  The role of dead_time
 * ---------------------------------------------------------------
 *
 *  dead_time is a safety gap added after each pulse before the next
 *  channel is allowed to switch on. It prevents two channels from
 *  conducting simultaneously, which would short the supply.
 *
 *  The constraint the driver enforces on every channel is:
 *
 *    pulse_width + dead_time <= slot
 *
 *  If any channel violates this, interleavedPWMCreate returns an
 *  error and nothing is created.
 *
 *  Worked example with the config used below:
 *
 *    time_period = 20 000 µs
 *    total_gpio  = 4
 *    slot        = 20 000 / 4 = 5 000 µs
 *    dead_time   = 1 000 µs
 *    max valid pulse_width = slot - dead_time = 4 000 µs
 *
 *    CH0: pulse=2000  → 2000 + 1000 = 3000 <= 5000  ✓
 *    CH1: pulse=3000  → 3000 + 1000 = 4000 <= 5000  ✓
 *    CH2: pulse=1500  → 1500 + 1000 = 2500 <= 5000  ✓
 *    CH3: pulse=4000  → 4000 + 1000 = 5000 <= 5000  ✓  (boundary)
 *
 *    CH3 is at the exact boundary — one microsecond wider and it
 *    would be rejected.
 *
 * ---------------------------------------------------------------
 *  What happens if the constraint is violated
 * ---------------------------------------------------------------
 *
 *  To show the validation in action, this example first attempts
 *  to create the instance with an invalid configuration (CH3 pulse
 *  one step over the boundary), observes the rejection, then creates
 *  it correctly.
 *
 * ---------------------------------------------------------------
 *  Hardware connections
 * ---------------------------------------------------------------
 *
 *    CH0 → GPIO 5
 *    CH1 → GPIO 18
 *    CH2 → GPIO 22
 *    CH3 → GPIO 23
 */

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "interleaved_pwm.h"

static const char *TAG = "pwm_getting_started";

void app_main(void)
{
    ESP_LOGI(TAG, "interleaved_pwm getting started example");

    static uint8_t gpio_list[4] = {5, 18, 22, 23};

    /* ---------------------------------------------------------- */
    /*  Step 1 — Demonstrate validation                           */
    /*                                                            */
    /*  CH3 pulse_width = 4001 µs                                 */
    /*  4001 + dead_time(1000) = 5001 > slot(5000)  → must fail  */
    /* ---------------------------------------------------------- */

    ESP_LOGI(TAG, "--- Step 1: Attempt creation with invalid config ---");
    ESP_LOGI(TAG, "  slot      = 20000 / 4 = 5000 us");
    ESP_LOGI(TAG, "  dead_time = 1000 us");
    ESP_LOGI(TAG, "  CH3 pulse = 4001 us  ->  4001 + 1000 = 5001 > 5000  (invalid)");

    static uint32_t invalid_widths[4] = {2000, 3000, 1500, 4001};

    interleaved_pwm_config_t invalid_config = {
        .gpio_no      = gpio_list,
        .pulse_widths = invalid_widths,
        .total_gpio   = 4,
        .dead_time    = 1000,
        .time_period  = 20000
    };

    interleaved_pwm_interface_t* pwm;
    int ret = interleavedPWMCreate(&invalid_config,&pwm);

    if (ret != 0)
    {
        ESP_LOGI(TAG, "Creation correctly rejected (ret=%d) -- constraint enforced", ret);
    }
    else
    {
        ESP_LOGW(TAG, "Unexpected success -- check your validation logic");
        PWM_DESTROY(&pwm);
    }

    /* ---------------------------------------------------------- */
    /*  Step 2 — Create with valid config                         */
    /*                                                            */
    /*  CH3 pulse_width = 4000 µs (exactly at the boundary)      */
    /*  4000 + dead_time(1000) = 5000 == slot(5000)  → valid     */
    /* ---------------------------------------------------------- */

    ESP_LOGI(TAG, "--- Step 2: Create with valid config ---");
    ESP_LOGI(TAG, "  CH0 pulse = 2000 us  ->  2000 + 1000 = 3000 <= 5000  OK");
    ESP_LOGI(TAG, "  CH1 pulse = 3000 us  ->  3000 + 1000 = 4000 <= 5000  OK");
    ESP_LOGI(TAG, "  CH2 pulse = 1500 us  ->  1500 + 1000 = 2500 <= 5000  OK");
    ESP_LOGI(TAG, "  CH3 pulse = 4000 us  ->  4000 + 1000 = 5000 <= 5000  OK (boundary)");

    static uint32_t valid_widths[4] = {2000, 3000, 1500, 4000};

    interleaved_pwm_config_t valid_config = {
        .gpio_no      = gpio_list,
        .pulse_widths = valid_widths,
        .total_gpio   = 4,
        .dead_time    = 1000,
        .time_period  = 20000
    };

    ret = interleavedPWMCreate(&valid_config,&pwm);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Creation failed unexpectedly (ret=%d)", ret);
        return;
    }
    ESP_LOGI(TAG, "Instance created successfully");

    /* ---------------------------------------------------------- */
    /*  Step 3 — Start                                            */
    /* ---------------------------------------------------------- */

    ESP_LOGI(TAG, "--- Step 3: Start ---");
    ret = PWM_START(pwm);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "PWM_START failed (ret=%d)", ret);
        PWM_DESTROY(&pwm);
        return;
    }
    ESP_LOGI(TAG, "PWM running -- observe GPIOs 5, 18, 22, 23 on a scope");
    ESP_LOGI(TAG, "Each channel is phase-shifted by 5000 us (one slot)");

    vTaskDelay(pdMS_TO_TICKS(5000));

    /* ---------------------------------------------------------- */
    /*  Step 4 — Stop and destroy                                 */
    /* ---------------------------------------------------------- */

    ESP_LOGI(TAG, "--- Step 4: Stop and destroy ---");
    PWM_STOP(pwm);
    ESP_LOGI(TAG, "PWM stopped -- all outputs idle");

    PWM_DESTROY(&pwm);
    ESP_LOGI(TAG, "Instance destroyed -- GPIOs released");

    ESP_LOGI(TAG, "Example complete");
}