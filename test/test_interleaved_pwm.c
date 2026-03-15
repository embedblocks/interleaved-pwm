/*
 * test_interleaved_pwm.c
 *
 * Comprehensive Unity test suite for the interleaved_pwm component.
 * Covers: creation, validation, lifecycle, state machine, edge cases,
 *         boundary conditions, and observable hardware tests.
 *
 * Test groups:
 *   [interleaved_pwm][create]      - Construction and validation
 *   [interleaved_pwm][lifecycle]   - Start / stop / destroy sequencing
 *   [interleaved_pwm][state]       - State machine / call-order violations
 *   [interleaved_pwm][boundary]    - Boundary and edge-case values
 *   [interleaved_pwm][multi]       - Multi-channel configurations
 *   [interleaved_pwm][manual]      - Hardware-observable tests (opt-in)
 */

#include <stdint.h>
#include <stdbool.h>
#include "unity.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "interleaved_pwm.h"

static const char *TAG = "test_interleaved_pwm";

/* Shared handle used across tests */
static interleaved_pwm_t interleaved_pwm;
static bool interleavedPWMCreated = false;

/* ------------------------------------------------------------------ */
/*  Unity hooks                                                        */
/* ------------------------------------------------------------------ */

void setUp(void)
{
    interleavedPWMCreated = false;
}

void tearDown(void)
{
    if (interleavedPWMCreated)
    {
        ESP_LOGI(TAG, "Tearing down interleaved_pwm");
        interleaved_pwm.interface.destroy(&interleaved_pwm.interface);
        interleavedPWMCreated = false;
    }
}

/* ------------------------------------------------------------------ */
/*  Helper builders                                                    */
/* ------------------------------------------------------------------ */

/*
 * Default 2-channel, 10 kHz (100 µs period) configuration.
 *   time_period  = 10 000 µs  → each channel slot = 5 000 µs
 *   pulse_width  =  2 000 µs  (40 % duty)
 *   dead_time    =  1 000 µs  → worst-case on-time = 3 000 µs < 5 000 µs  ✓
 */
static void create_default_interleaved_pwm(void)
{
    static uint8_t  gpio_list[2]    = {5, 18};
    static uint32_t pulse_widths[2] = {2000, 2000};

    interleaved_pwm_config_t config = {
        .gpio_no     = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio  = 2,
        .dead_time   = 1000,
        .time_period = 10000
    };

    int ret = interleavedPWMCreate(&interleaved_pwm, &config);
    ESP_LOGI(TAG, "create_default return %d", ret);
    TEST_ASSERT_EQUAL(0, ret);
    interleavedPWMCreated = true;
}

/*
 * 4-channel variant used by multi-channel tests.
 *   time_period  = 20 000 µs  → each channel slot = 5 000 µs
 *   pulse_width  =  2 000 µs  (40 % per channel)
 *   dead_time    =  1 000 µs
 */
static void create_4ch_interleaved_pwm(void)
{
    static uint8_t  gpio_list[4]    = {5, 18, 22, 23};
    static uint32_t pulse_widths[4] = {2000, 2000, 2000, 2000};

    interleaved_pwm_config_t config = {
        .gpio_no     = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio  = 4,
        .dead_time   = 1000,
        .time_period = 20000
    };

    int ret = interleavedPWMCreate(&interleaved_pwm, &config);
    ESP_LOGI(TAG, "create_4ch return %d", ret);
    TEST_ASSERT_EQUAL(0, ret);
    interleavedPWMCreated = true;
}

/* ================================================================== */
/*  GROUP 1 – Creation and input validation                           */
/* ================================================================== */

TEST_CASE("create: succeeds with valid 2-channel configuration",
          "[interleaved_pwm][create]")
{
    create_default_interleaved_pwm();
}

/* --- NULL / missing arguments ------------------------------------ */

TEST_CASE("create: fails when handle pointer is NULL",
          "[interleaved_pwm][create]")
{
    static uint8_t  gpio_list[2]    = {5, 18};
    static uint32_t pulse_widths[2] = {2000, 2000};

    interleaved_pwm_config_t config = {
        .gpio_no     = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio  = 2,
        .dead_time   = 1000,
        .time_period = 10000
    };

    int ret = interleavedPWMCreate(NULL, &config);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

TEST_CASE("create: fails when config pointer is NULL",
          "[interleaved_pwm][create]")
{
    int ret = interleavedPWMCreate(&interleaved_pwm, NULL);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

TEST_CASE("create: fails when gpio_no is NULL",
          "[interleaved_pwm][create]")
{
    static uint32_t pulse_widths[2] = {2000, 2000};

    interleaved_pwm_config_t config = {
        .gpio_no     = NULL,        /* <-- invalid */
        .pulse_widths = pulse_widths,
        .total_gpio  = 2,
        .dead_time   = 1000,
        .time_period = 10000
    };

    int ret = interleavedPWMCreate(&interleaved_pwm, &config);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

TEST_CASE("create: fails when pulse_widths is NULL",
          "[interleaved_pwm][create]")
{
    static uint8_t gpio_list[2] = {5, 18};

    interleaved_pwm_config_t config = {
        .gpio_no     = gpio_list,
        .pulse_widths = NULL,       /* <-- invalid */
        .total_gpio  = 2,
        .dead_time   = 1000,
        .time_period = 10000
    };

    int ret = interleavedPWMCreate(&interleaved_pwm, &config);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

/* --- total_gpio edge cases --------------------------------------- */

TEST_CASE("create: fails when total_gpio is zero",
          "[interleaved_pwm][create]")
{
    static uint8_t  gpio_list[2]    = {5, 18};
    static uint32_t pulse_widths[2] = {2000, 2000};

    interleaved_pwm_config_t config = {
        .gpio_no     = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio  = 0,           /* <-- invalid */
        .dead_time   = 1000,
        .time_period = 10000
    };

    int ret = interleavedPWMCreate(&interleaved_pwm, &config);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

TEST_CASE("create: single gpio channel accepted or rejected per spec",
          "[interleaved_pwm][create]")
{
    /*
     * Whether total_gpio = 1 is valid depends on your design contract.
     * Adjust the expected value (0 or non-0) to match your specification.
     * This test documents the decision and prevents silent regression.
     */
    static uint8_t  gpio_list[1]    = {5};
    static uint32_t pulse_widths[1] = {2000};

    interleaved_pwm_config_t config = {
        .gpio_no     = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio  = 1,
        .dead_time   = 1000,
        .time_period = 10000
    };

    int ret = interleavedPWMCreate(&interleaved_pwm, &config);

    /* CHOOSE ONE and delete the other: */
    TEST_ASSERT_EQUAL(0, ret);      /* valid: single-channel allowed   */
    /* TEST_ASSERT_NOT_EQUAL(0, ret); */  /* invalid: must have >= 2 channels */

    if (ret == 0) { interleavedPWMCreated = true; }
}

/* --- time_period edge cases ------------------------------------- */

TEST_CASE("create: fails when time_period is zero",
          "[interleaved_pwm][create]")
{
    static uint8_t  gpio_list[2]    = {5, 18};
    static uint32_t pulse_widths[2] = {2000, 2000};

    interleaved_pwm_config_t config = {
        .gpio_no     = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio  = 2,
        .dead_time   = 0,
        .time_period = 0            /* <-- invalid */
    };

    int ret = interleavedPWMCreate(&interleaved_pwm, &config);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

/* --- dead_time edge cases --------------------------------------- */

TEST_CASE("create: zero dead_time is accepted",
          "[interleaved_pwm][create]")
{
    /*
     * A dead_time of 0 means no blanking gap between phases.
     * This may be useful at low switching frequencies.
     * Adjust expectation if your hardware requires dead_time > 0.
     */
    static uint8_t  gpio_list[2]    = {5, 18};
    static uint32_t pulse_widths[2] = {2000, 2000};

    interleaved_pwm_config_t config = {
        .gpio_no     = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio  = 2,
        .dead_time   = 0,           /* no blanking */
        .time_period = 10000
    };

    int ret = interleavedPWMCreate(&interleaved_pwm, &config);
    TEST_ASSERT_EQUAL(0, ret);
    interleavedPWMCreated = true;
}

/* --- pulse_width vs period validation ---------------------------- */

/*
 * Interleaved slot maths for N channels:
 *   slot_width = time_period / N
 *   constraint: pulse_width + dead_time <= slot_width
 *
 * With N=2, period=10000:
 *   slot = 5000 µs
 *   pulse=6000 + dead_time=2000 = 8000 > 5000  → must FAIL
 */
TEST_CASE("create: fails when pulse_width plus dead_time exceeds channel slot",
          "[interleaved_pwm][create]")
{
    static uint8_t  gpio_list[2]    = {5, 18};
    static uint32_t pulse_widths[2] = {6000, 6000};

    interleaved_pwm_config_t config = {
        .gpio_no     = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio  = 2,
        .dead_time   = 2000,
        .time_period = 10000
    };

    int ret = interleavedPWMCreate(&interleaved_pwm, &config);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

/*
 * Boundary: pulse_width + dead_time == slot_width exactly.
 * Adjust expectation to 0 if your spec allows equal-to, or NOT_EQUAL
 * if the constraint is strictly less-than.
 */
TEST_CASE("create: pulse_width plus dead_time equal to slot is boundary condition",
          "[interleaved_pwm][create]")
{
    /* slot = 10000 / 2 = 5000; pulse(3000) + dead_time(2000) == 5000 */
    static uint8_t  gpio_list[2]    = {5, 18};
    static uint32_t pulse_widths[2] = {3000, 3000};

    interleaved_pwm_config_t config = {
        .gpio_no     = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio  = 2,
        .dead_time   = 2000,
        .time_period = 10000
    };

    int ret = interleavedPWMCreate(&interleaved_pwm, &config);

    /* CHOOSE ONE: strict-less → NOT_EQUAL; less-or-equal → EQUAL */
    TEST_ASSERT_EQUAL(0, ret);
    if (ret == 0) { interleavedPWMCreated = true; }
}

/*
 * One channel over-budget, others fine: the whole create should fail.
 */
TEST_CASE("create: fails when any single channel pulse_width exceeds its slot",
          "[interleaved_pwm][create]")
{
    static uint8_t  gpio_list[2]    = {5, 18};
    static uint32_t pulse_widths[2] = {2000, 9000}; /* ch1 valid, ch2 too wide */

    interleaved_pwm_config_t config = {
        .gpio_no     = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio  = 2,
        .dead_time   = 1000,
        .time_period = 10000
    };

    int ret = interleavedPWMCreate(&interleaved_pwm, &config);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

/* ================================================================== */
/*  GROUP 2 – Normal lifecycle                                        */
/* ================================================================== */

TEST_CASE("lifecycle: start returns success",
          "[interleaved_pwm][lifecycle]")
{
    create_default_interleaved_pwm();
    int ret = interleaved_pwm.interface.start(&interleaved_pwm.interface);
    TEST_ASSERT_EQUAL(0, ret);
}

TEST_CASE("lifecycle: stop after start returns success",
          "[interleaved_pwm][lifecycle]")
{
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.start(&interleaved_pwm.interface));
    int ret = interleaved_pwm.interface.stop(&interleaved_pwm.interface);
    TEST_ASSERT_EQUAL(0, ret);
}

TEST_CASE("lifecycle: destroy returns success and releases resources",
          "[interleaved_pwm][lifecycle]")
{
    create_default_interleaved_pwm();
    int ret = interleaved_pwm.interface.destroy(&interleaved_pwm.interface);
    TEST_ASSERT_EQUAL(0, ret);
    interleavedPWMCreated = false;   /* prevent double-destroy in tearDown */
}

TEST_CASE("lifecycle: full start-stop-destroy sequence succeeds",
          "[interleaved_pwm][lifecycle]")
{
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.start(&interleaved_pwm.interface));
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.stop(&interleaved_pwm.interface));
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.destroy(&interleaved_pwm.interface));
    interleavedPWMCreated = false;
}

TEST_CASE("lifecycle: start-stop cycle can be repeated",
          "[interleaved_pwm][lifecycle]")
{
    create_default_interleaved_pwm();

    for (int i = 0; i < 5; i++)
    {
        ESP_LOGI(TAG, "Start/stop cycle %d", i + 1);
        TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.start(&interleaved_pwm.interface));
        TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.stop(&interleaved_pwm.interface));
    }
}

/* ================================================================== */
/*  GROUP 3 – State machine / call-order violations                   */
/* ================================================================== */

TEST_CASE("state: stop without prior start is defined behaviour",
          "[interleaved_pwm][state]")
{
    /*
     * Calling stop on a created-but-not-started handle must not crash.
     * Expected result is implementation-defined:
     *   - 0 (idempotent / benign no-op)  ← most forgiving
     *   - non-zero (error: wrong state)  ← stricter state machine
     * Pick one and document the contract.
     */
    create_default_interleaved_pwm();
    int ret = interleaved_pwm.interface.stop(&interleaved_pwm.interface);

    /* CHOOSE ONE: */
    TEST_ASSERT_EQUAL(0, ret);          /* tolerant: no-op is fine  */
    /* TEST_ASSERT_NOT_EQUAL(0, ret); */ /* strict:  must start first */
}

TEST_CASE("state: calling start twice is idempotent or returns error",
          "[interleaved_pwm][state]")
{
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.start(&interleaved_pwm.interface));

    int ret = interleaved_pwm.interface.start(&interleaved_pwm.interface);

    /* CHOOSE ONE: */
    TEST_ASSERT_EQUAL(0, ret);          /* tolerant: second start is a no-op */
    /* TEST_ASSERT_NOT_EQUAL(0, ret); */ /* strict:  must stop before restart  */
}

TEST_CASE("state: calling stop twice is idempotent or returns error",
          "[interleaved_pwm][state]")
{
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.start(&interleaved_pwm.interface));
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.stop(&interleaved_pwm.interface));

    int ret = interleaved_pwm.interface.stop(&interleaved_pwm.interface);

    /* CHOOSE ONE: */
    TEST_ASSERT_EQUAL(0, ret);
    /* TEST_ASSERT_NOT_EQUAL(0, ret); */
}

TEST_CASE("state: destroy while running stops and cleans up safely",
          "[interleaved_pwm][state]")
{
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.start(&interleaved_pwm.interface));

    /* Must not crash or leak even if stop was never called */
    int ret = interleaved_pwm.interface.destroy(&interleaved_pwm.interface);
    TEST_ASSERT_EQUAL(0, ret);
    interleavedPWMCreated = false;
}

TEST_CASE("state: operations after destroy return error",
          "[interleaved_pwm][state]")
{
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.destroy(&interleaved_pwm.interface));
    interleavedPWMCreated = false;

    /* All interface calls on a destroyed handle must return an error */
    TEST_ASSERT_NOT_EQUAL(0, interleaved_pwm.interface.start(&interleaved_pwm.interface));
    TEST_ASSERT_NOT_EQUAL(0, interleaved_pwm.interface.stop(&interleaved_pwm.interface));
}

/* ================================================================== */
/*  GROUP 4 – Multi-channel configurations                            */
/* ================================================================== */

TEST_CASE("multi: 4-channel configuration is created successfully",
          "[interleaved_pwm][multi]")
{
    create_4ch_interleaved_pwm();
}

TEST_CASE("multi: 4-channel start and stop succeeds",
          "[interleaved_pwm][multi]")
{
    create_4ch_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.start(&interleaved_pwm.interface));
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.stop(&interleaved_pwm.interface));
}

TEST_CASE("multi: 4-channel with mismatched pulse widths succeeds",
          "[interleaved_pwm][multi]")
{
    /*
     * Real converters often need different duty cycles per phase.
     * Each pulse must still fit its own slot independently.
     *   slot = 20000 / 4 = 5000 µs
     *   All widths below are < 5000 - dead_time(1000) = 4000  ✓
     */
    static uint8_t  gpio_list[4]    = {5, 18, 22, 23};
    static uint32_t pulse_widths[4] = {1000, 2000, 3000, 1500};

    interleaved_pwm_config_t config = {
        .gpio_no     = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio  = 4,
        .dead_time   = 1000,
        .time_period = 20000
    };

    int ret = interleavedPWMCreate(&interleaved_pwm, &config);
    TEST_ASSERT_EQUAL(0, ret);
    interleavedPWMCreated = true;
}

TEST_CASE("multi: 4-channel fails when one channel pulse_width exceeds its slot",
          "[interleaved_pwm][multi]")
{
    /*
     * slot = 20000 / 4 = 5000; ch2 pulse(4500) + dead_time(1000) = 5500 > 5000  → FAIL
     */
    static uint8_t  gpio_list[4]    = {5, 18, 22, 23};
    static uint32_t pulse_widths[4] = {1000, 4500, 1000, 1000};

    interleaved_pwm_config_t config = {
        .gpio_no     = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio  = 4,
        .dead_time   = 1000,
        .time_period = 20000
    };

    int ret = interleavedPWMCreate(&interleaved_pwm, &config);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

/* ================================================================== */
/*  GROUP 5 – Re-creation after destroy                               */
/* ================================================================== */

TEST_CASE("recreate: handle can be reused after destroy",
          "[interleaved_pwm][lifecycle]")
{
    /* First instance */
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.start(&interleaved_pwm.interface));
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.stop(&interleaved_pwm.interface));
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.destroy(&interleaved_pwm.interface));
    interleavedPWMCreated = false;

    /* Re-create on the same handle — must not retain stale state */
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.start(&interleaved_pwm.interface));
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.stop(&interleaved_pwm.interface));
}

/* ================================================================== */
/*  GROUP 6 – Manual / hardware-observable tests  (opt-in)           */
/*                                                                     */
/*  Run with:                                                          */
/*    idf.py test -f "[manual]"                                        */
/*  Exclude with:                                                      */
/*    idf.py test -e "[manual]"                                        */
/* ================================================================== */

TEST_CASE("manual: 2-channel waveform observable on oscilloscope",
          "[interleaved_pwm][manual]")
{
    /*
     * Expected on scope:
     *   CH1 (GPIO5):  2 000 µs high, 1 000 µs dead, remainder low
     *   CH2 (GPIO18): phase-shifted by period/2 = 5 000 µs
     *   Frequency: 100 Hz (10 000 µs period)
     */
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.start(&interleaved_pwm.interface));
    ESP_LOGI(TAG, "2-ch waveform running for 3 s — observe GPIOs 5 and 18");
    vTaskDelay(pdMS_TO_TICKS(3000));
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.stop(&interleaved_pwm.interface));
}

TEST_CASE("manual: 4-channel waveform observable on oscilloscope",
          "[interleaved_pwm][manual]")
{
    /*
     * Expected on scope:
     *   4 channels, each phase-shifted by period/4 = 5 000 µs
     *   Pulse: 2 000 µs high, 1 000 µs dead
     *   Period: 20 000 µs (50 Hz)
     */
    create_4ch_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.start(&interleaved_pwm.interface));
    ESP_LOGI(TAG, "4-ch waveform running for 3 s — observe GPIOs 5, 18, 22, 23");
    vTaskDelay(pdMS_TO_TICKS(3000));
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.stop(&interleaved_pwm.interface));
}

TEST_CASE("manual: waveform stops cleanly — no residual output after stop",
          "[interleaved_pwm][manual]")
{
    /*
     * After stop(), all GPIO outputs must be driven low (or to a defined
     * idle level). Verify with scope that no pulses appear after stop.
     */
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.start(&interleaved_pwm.interface));
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(0, interleaved_pwm.interface.stop(&interleaved_pwm.interface));
    ESP_LOGI(TAG, "Outputs stopped — verify GPIOs are idle for 2 s");
    vTaskDelay(pdMS_TO_TICKS(2000));
}