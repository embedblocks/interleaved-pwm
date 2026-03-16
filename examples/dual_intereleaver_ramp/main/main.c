/*
 * main.c — Interleaved PWM example
 *
 * Demonstrates:
 *   1. Basic create / start / stop
 *   2. Runtime duty ramp (up and down in a loop)
 *   3. Multiple independent instances
 *   4. Power saving via stop + destroy
 *
 * Hardware:
 *   Instance A — boost converter (4 channels)
 *     CH0 → GPIO 5
 *     CH1 → GPIO 18
 *     CH2 → GPIO 22
 *     CH3 → GPIO 23
 *
 *   Instance B — auxiliary converter (4 channels)
 *     CH0 → GPIO 25
 *     CH1 → GPIO 26
 *     CH2 → GPIO 27
 *     CH3 → GPIO 32
 *
 * Timing (both instances):
 *   time_period = 20 000 µs  (50 Hz)
 *   dead_time   =  1 000 µs
 *   slot        =  5 000 µs  (20 000 / 4 channels)
 *   max width   =  4 000 µs  (slot - dead_time)
 *   ramp range  =  500 µs – 4 000 µs
 */

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "interleaved_pwm.h"

static const char *TAG = "pwm_example";

/* ------------------------------------------------------------------ */
/*  Configuration                                                      */
/* ------------------------------------------------------------------ */

#define TIME_PERIOD_US   20000u   /* full switching period            */
#define DEAD_TIME_US      1000u   /* blanking gap between phases      */
#define NUM_CHANNELS         4u   /* phases per instance              */

/* Slot = TIME_PERIOD_US / NUM_CHANNELS = 5000 µs
   Valid pulse range: 1 µs  to  slot - dead_time = 4000 µs           */
#define RAMP_MIN_US        500u
#define RAMP_MAX_US       4000u
#define RAMP_STEP_US       100u
#define RAMP_STEP_MS        50u   /* delay between each step          */

#define HOLD_MS           2000u   /* pause at top and bottom of ramp  */
#define IDLE_MS           3000u   /* both instances off between cycles */

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

/*
 * Create and start a 4-channel interleaved PWM instance.
 * All channels start at the same initial pulse width.
 */
static esp_err_t converter_init(interleaved_pwm_t *pwm,
                                uint8_t           *gpio_list,
                                uint32_t           initial_width_us)
{
    /* Each channel gets the same starting width */
    uint32_t pulse_widths[NUM_CHANNELS];
    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        pulse_widths[i] = initial_width_us;
    }

    interleaved_pwm_config_t config = {
        .gpio_no      = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio   = NUM_CHANNELS,
        .dead_time    = DEAD_TIME_US,
        .time_period  = TIME_PERIOD_US
    };

    int ret = interleavedPWMCreate(pwm, &config);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "interleavedPWMCreate failed (%d)", ret);
        return ESP_FAIL;
    }

    ret = PWM_START(pwm);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "PWM_START failed (%d)", ret);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Converter started — initial width %lu µs", initial_width_us);
    return ESP_OK;
}

/*
 * Ramp all channels of an instance from current_width up to RAMP_MAX_US
 * then back down to RAMP_MIN_US.
 * Both instances are updated in lock-step each step so their duty
 * tracks together for a symmetric dual-converter demonstration.
 */
static void ramp_both(interleaved_pwm_t *pwm_a,
                      interleaved_pwm_t *pwm_b)
{
    uint32_t width = RAMP_MIN_US;
    int      direction = 1;   /* 1 = ramping up, -1 = ramping down */

    ESP_LOGI(TAG, "--- Ramp start ---");

    while (1)
    {
        /* Apply new width to every channel on both instances */
        for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++)
        {
            int ret_a = PWM_SET_WIDTH(pwm_a, ch, width);
            int ret_b = PWM_SET_WIDTH(pwm_b, ch, width);

            if (ret_a != 0 || ret_b != 0)
            {
                ESP_LOGW(TAG, "SET_WIDTH rejected on ch%u width=%lu µs (a=%d b=%d)",
                         ch, width, ret_a, ret_b);
            }
        }

        ESP_LOGI(TAG, "Width → %4lu µs", width);
        vTaskDelay(pdMS_TO_TICKS(RAMP_STEP_MS));

        /* Advance ramp */
        if (direction == 1)
        {
            if (width + RAMP_STEP_US >= RAMP_MAX_US)
            {
                width = RAMP_MAX_US;
                ESP_LOGI(TAG, "Peak reached — holding for %u ms", HOLD_MS);
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
                ESP_LOGI(TAG, "Valley reached — ramp complete");
                break;
            }
            else
            {
                width -= RAMP_STEP_US;
            }
        }
    }
}

/*
 * Gracefully stop and destroy an instance, demonstrating the power
 * saving path: stop halts switching, destroy releases the peripheral
 * and resets the GPIO lines.
 */
static void converter_shutdown(interleaved_pwm_t *pwm, const char *name)
{
    ESP_LOGI(TAG, "Shutting down %s", name);

    int ret = PWM_STOP(pwm);
    if (ret != 0)
    {
        ESP_LOGW(TAG, "%s PWM_STOP returned %d", name, ret);
    }

    ret = PWM_DESTROY(pwm);
    if (ret != 0)
    {
        ESP_LOGW(TAG, "%s PWM_DESTROY returned %d", name, ret);
    }

    ESP_LOGI(TAG, "%s offline — GPIOs reset, peripheral released", name);
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "Interleaved PWM example starting");
    ESP_LOGI(TAG, "Period %u µs | Dead time %u µs | %u channels per instance",
             TIME_PERIOD_US, DEAD_TIME_US, NUM_CHANNELS);

    /* GPIO assignments */
    static uint8_t gpio_a[NUM_CHANNELS] = {5,  18, 22, 23};
    static uint8_t gpio_b[NUM_CHANNELS] = {25, 26, 27, 32};

    interleaved_pwm_t boost_converter;
    interleaved_pwm_t aux_converter;

    int cycle = 0;

    while (1)
    {
        cycle++;
        ESP_LOGI(TAG, "======== Cycle %d ========", cycle);

        /* ---------------------------------------------------------- */
        /*  1. Create and start both instances                        */
        /* ---------------------------------------------------------- */

        ESP_LOGI(TAG, "Initialising boost converter (GPIOs 5,18,22,23)");
        if (converter_init(&boost_converter, gpio_a, RAMP_MIN_US) != ESP_OK)
        {
            ESP_LOGE(TAG, "Boost converter init failed — halting");
            return;
        }

        ESP_LOGI(TAG, "Initialising aux converter   (GPIOs 25,26,27,32)");
        if (converter_init(&aux_converter, gpio_b, RAMP_MIN_US) != ESP_OK)
        {
            ESP_LOGE(TAG, "Aux converter init failed — halting");
            converter_shutdown(&boost_converter, "boost");
            return;
        }

        /* ---------------------------------------------------------- */
        /*  2. Ramp duty on both instances simultaneously             */
        /* ---------------------------------------------------------- */

        ramp_both(&boost_converter, &aux_converter);

        /* ---------------------------------------------------------- */
        /*  3. Stop and destroy — power saving / resource release     */
        /* ---------------------------------------------------------- */

        converter_shutdown(&boost_converter, "boost");
        converter_shutdown(&aux_converter,   "aux  ");

        /* ---------------------------------------------------------- */
        /*  4. Idle period — both instances off, no switching         */
        /* ---------------------------------------------------------- */

        ESP_LOGI(TAG, "Both converters offline — idle for %u ms", IDLE_MS);
        vTaskDelay(pdMS_TO_TICKS(IDLE_MS));

        /* Loop back and re-create both instances from scratch,
           demonstrating that destroy + re-create works correctly    */
    }
}