/*
 * main.c — Interleaved PWM example
 *
 * Demonstrates:
 *   1. Basic create / start / stop
 *   2. Runtime duty ramp (up and down in a loop)
 *   3. Power saving via stop + destroy
 *
 * NOTE:
 *   This version supports only ONE interleaved PWM instance.
 *
 * Hardware:
 *   4-channel interleaved PWM
 *     CH0 → GPIO 5
 *     CH1 → GPIO 18
 *     CH2 → GPIO 22
 *     CH3 → GPIO 23
 *
 * Timing:
 *   time_period = 20 000 µs  (50 Hz)
 *   dead_time   =  1 000 µs
 *   slot        =  5 000 µs  (20 000 / 4 channels)
 *   max width   =  4 000 µs  (slot - dead_time)
 *   ramp range  =  500 µs – 4 000 µs
 */

#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "interleaved_pwm.h"

static const char *TAG = "pwm_example";

/* ------------------------------------------------------------------ */
/*  Configuration                                                      */
/* ------------------------------------------------------------------ */

#define TIME_PERIOD_US   20000u
#define DEAD_TIME_US      1000u
#define NUM_CHANNELS         4u

#define RAMP_MIN_US        500u
#define RAMP_MAX_US       4000u
#define RAMP_STEP_US       100u
#define RAMP_STEP_MS        50u

#define HOLD_MS           2000u
#define IDLE_MS           3000u

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static esp_err_t converter_init(interleaved_pwm_interface_t **pwm,
                                uint8_t           *gpio_list,
                                uint32_t           initial_width_us)
{
    uint32_t pulse_widths[NUM_CHANNELS];

    for (int i = 0; i < NUM_CHANNELS; i++)
        pulse_widths[i] = initial_width_us;

    interleaved_pwm_config_t config = {
        .gpio_no      = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio   = NUM_CHANNELS,
        .dead_time    = DEAD_TIME_US,
        .time_period  = TIME_PERIOD_US
        .idle_state = INTERLEAVED_PWM_IDLE_STATE_LOW
    };

    esp_err_t ret = interleavedPWMCreate(&config,pwm);
    if (ret!=0)
    {
        ESP_LOGE(TAG, "interleavedPWMCreate failed");
        return ESP_FAIL;
    }

    ret = PWM_START(*pwm);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "PWM_START failed (%d)", ret);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "PWM started — initial width %lu µs", initial_width_us);
    return ESP_OK;
}

/*
 * Ramp all channels up and down
 */
static void ramp(interleaved_pwm_interface_t *pwm)
{
    uint32_t width = RAMP_MIN_US;
    int direction = 1;

    ESP_LOGI(TAG, "--- Ramp start ---");

    while (1)
    {
        for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++)
        {
            int ret = PWM_SET_WIDTH(pwm, ch, width);

            if (ret != 0)
            {
                ESP_LOGW(TAG, "SET_WIDTH rejected ch%u width=%lu (%d)",
                         ch, width, ret);
            }
        }

        ESP_LOGI(TAG, "Width → %4lu µs", width);
        vTaskDelay(pdMS_TO_TICKS(RAMP_STEP_MS));

        if (direction == 1)
        {
            if (width + RAMP_STEP_US >= RAMP_MAX_US)
            {
                width = RAMP_MAX_US;
                vTaskDelay(pdMS_TO_TICKS(HOLD_MS));
                direction = -1;
            }
            else
            {
                width += RAMP_STEP_US;
            }
        }
        else
        {
            if (width <= RAMP_MIN_US + RAMP_STEP_US)
            {
                width = RAMP_MIN_US;
                break;
            }
            else
            {
                width -= RAMP_STEP_US;
            }
        }
    }
}

static void converter_shutdown(interleaved_pwm_interface_t *pwm)
{
    ESP_LOGI(TAG, "Shutting down PWM");

    PWM_STOP(pwm);
    PWM_DESTROY(&pwm);

    ESP_LOGI(TAG, "PWM stopped and resources released");
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "Interleaved PWM example starting");

    static uint8_t gpio[NUM_CHANNELS] = {5, 18, 22, 23};

    interleaved_pwm_interface_t* pwm;

    int cycle = 0;

    while (1)
    {
        cycle++;
        ESP_LOGI(TAG, "======== Cycle %d ========", cycle);

        if (converter_init(&pwm, gpio, RAMP_MIN_US) != ESP_OK)
        {
            ESP_LOGE(TAG, "Init failed — halting");
            return;
        }

        ramp(pwm);

        converter_shutdown(pwm);

        ESP_LOGI(TAG, "Idle for %u ms", IDLE_MS);
        vTaskDelay(pdMS_TO_TICKS(IDLE_MS));
    }
}