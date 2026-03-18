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
static interleaved_pwm_interface_t* interleaved_pwm;
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
        PWM_DESTROY(&interleaved_pwm);
        //ESP_LOGI(TAG,"interface address in tear down %p",interleaved_pwm);
        //interleaved_pwm->destroy(&interleaved_pwm);
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

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    ESP_LOGI(TAG, "create reult %d", ret);
    TEST_ASSERT_NOT_NULL(interleaved_pwm);
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

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    ESP_LOGI(TAG, "create_4ch result %d", ret);
    TEST_ASSERT_NOT_NULL(interleaved_pwm);
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



TEST_CASE("create: fails when config pointer is NULL",
          "[interleaved_pwm][create]")
{
    esp_err_t ret=interleavedPWMCreate(NULL,&interleaved_pwm);
    TEST_ASSERT_NULL(interleaved_pwm);
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

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NULL(interleaved_pwm);
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

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NULL(interleaved_pwm);
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

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NULL(interleaved_pwm);
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

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);

    /* CHOOSE ONE and delete the other: */
    TEST_ASSERT_NOT_NULL(interleaved_pwm);      /* valid: single-channel allowed   */
    /* TEST_ASSERT_NULL(interleaved_pwm); */  /* invalid: must have >= 2 channels */

    if(interleaved_pwm!=NULL){ interleavedPWMCreated = true; }
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

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NULL(interleaved_pwm);
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

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NOT_NULL(interleaved_pwm);
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

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NULL(interleaved_pwm);
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

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);

    /* CHOOSE ONE: strict-less → NOT_EQUAL; less-or-equal → EQUAL */
    TEST_ASSERT_NOT_NULL(interleaved_pwm);
    if(interleaved_pwm!=NULL){ interleavedPWMCreated = true; }
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

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NULL(interleaved_pwm);
}

/* ================================================================== */
/*  GROUP 2 – Normal lifecycle                                        */
/* ================================================================== */

TEST_CASE("lifecycle: start returns success",
          "[interleaved_pwm][lifecycle]")
{
    create_default_interleaved_pwm();
    int ret = PWM_START(interleaved_pwm);
    TEST_ASSERT_EQUAL(0,ret);
}

TEST_CASE("lifecycle: stop after start returns success",
          "[interleaved_pwm][lifecycle]")
{
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, PWM_START(interleaved_pwm));
    int ret = PWM_STOP(interleaved_pwm);
    TEST_ASSERT_EQUAL(0,ret);
}

TEST_CASE("lifecycle: destroy returns success and releases resources",
          "[interleaved_pwm][lifecycle]")
{
    create_default_interleaved_pwm();
    int ret = PWM_DESTROY(&interleaved_pwm);
    TEST_ASSERT_EQUAL(0,ret);
    interleavedPWMCreated = false;   /* prevent double-destroy in tearDown */
}

TEST_CASE("lifecycle: full start-stop-destroy sequence succeeds",
          "[interleaved_pwm][lifecycle]")
{
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, PWM_START(interleaved_pwm));
    TEST_ASSERT_EQUAL(0, PWM_STOP(interleaved_pwm));
    TEST_ASSERT_EQUAL(0, PWM_DESTROY(&interleaved_pwm));
    interleavedPWMCreated = false;
}

TEST_CASE("lifecycle: start-stop cycle can be repeated",
          "[interleaved_pwm][lifecycle]")
{
    create_default_interleaved_pwm();

    for (int i = 0; i < 5; i++)
    {
        ESP_LOGI(TAG, "Start/stop cycle %d", i + 1);
        TEST_ASSERT_EQUAL(0, PWM_START(interleaved_pwm));
        TEST_ASSERT_EQUAL(0, PWM_STOP(interleaved_pwm));
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
    int ret = PWM_STOP(interleaved_pwm);

    /* CHOOSE ONE: */
    TEST_ASSERT_EQUAL(0,ret);          /* tolerant: no-op is fine  */
    /* TEST_ASSERT_NULL(interleaved_pwm); */ /* strict:  must start first */
}

TEST_CASE("state: calling start twice is idempotent or returns error",
          "[interleaved_pwm][state]")
{
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, PWM_START(interleaved_pwm));

    int ret = PWM_START(interleaved_pwm);

    /* CHOOSE ONE: */
    TEST_ASSERT_EQUAL(0,ret);             /* tolerant: second start is a no-op */
    /* TEST_ASSERT_NULL(interleaved_pwm); */ /* strict:  must stop before restart  */
}

TEST_CASE("state: calling stop twice is idempotent or returns error",
          "[interleaved_pwm][state]")
{
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, PWM_START(interleaved_pwm));
    TEST_ASSERT_EQUAL(0, PWM_STOP(interleaved_pwm));

    int ret = PWM_STOP(interleaved_pwm);

    /* CHOOSE ONE: */
    TEST_ASSERT_EQUAL(0,ret);          /* tolerant: no-op is fine  */
    /* TEST_ASSERT_NULL(interleaved_pwm); */
}

TEST_CASE("state: destroy while running stops and cleans up safely",
          "[interleaved_pwm][state]")
{
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, PWM_START(interleaved_pwm));

    /* Must not crash or leak even if stop was never called */
    int ret = PWM_DESTROY(&interleaved_pwm);
    TEST_ASSERT_EQUAL(0,ret);
    interleavedPWMCreated = false;
}

TEST_CASE("state: operations after destroy return error",
          "[interleaved_pwm][state]")
{
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, PWM_DESTROY(&interleaved_pwm));
    interleavedPWMCreated = false;

    /* All interface calls on a destroyed handle must return an error */
    TEST_ASSERT_NOT_EQUAL(0, PWM_START(interleaved_pwm));
    TEST_ASSERT_NOT_EQUAL(0, PWM_STOP(interleaved_pwm));
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
    TEST_ASSERT_EQUAL(0, PWM_START(interleaved_pwm));
    TEST_ASSERT_EQUAL(0, PWM_STOP(interleaved_pwm));
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

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NOT_NULL(interleaved_pwm);
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

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NULL(interleaved_pwm);
}

/* ================================================================== */
/*  GROUP 5 – Re-creation after destroy                               */
/* ================================================================== */

TEST_CASE("recreate: handle can be reused after destroy",
          "[interleaved_pwm][lifecycle]")
{
    /* First instance */
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, PWM_START(interleaved_pwm));
    TEST_ASSERT_EQUAL(0, PWM_STOP(interleaved_pwm));
    TEST_ASSERT_EQUAL(0, PWM_DESTROY(&interleaved_pwm));
    interleavedPWMCreated = false;

    /* Re-create on the same handle — must not retain stale state */
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, PWM_START(interleaved_pwm));
    TEST_ASSERT_EQUAL(0, PWM_STOP(interleaved_pwm));
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
    TEST_ASSERT_EQUAL(0, PWM_START(interleaved_pwm));
    ESP_LOGI(TAG, "2-ch waveform running for 3 s — observe GPIOs 5 and 18");
    vTaskDelay(pdMS_TO_TICKS(3000));
    TEST_ASSERT_EQUAL(0, PWM_STOP(interleaved_pwm));
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
    TEST_ASSERT_EQUAL(0, PWM_START(interleaved_pwm));
    ESP_LOGI(TAG, "4-ch waveform running for 3 s — observe GPIOs 5, 18, 22, 23");
    vTaskDelay(pdMS_TO_TICKS(3000));
    TEST_ASSERT_EQUAL(0, PWM_STOP(interleaved_pwm));
}

TEST_CASE("manual: waveform stops cleanly — no residual output after stop",
          "[interleaved_pwm][manual]")
{
    /*
     * After stop(), all GPIO outputs must be driven low (or to a defined
     * idle level). Verify with scope that no pulses appear after stop.
     */
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, PWM_START(interleaved_pwm));
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(0, PWM_STOP(interleaved_pwm));
    ESP_LOGI(TAG, "Outputs stopped — verify GPIOs are idle for 2 s");
    vTaskDelay(pdMS_TO_TICKS(2000));
}


/* ================================================================== */
/*  GROUP 7 – changeWidth                                             */
/*                                                                     */
/*  Slot constraint (same as creation):                               */
/*    slot = time_period / total_gpio                                  */
/*    pulse_width + dead_time <= slot                                  */
/*                                                                     */
/*  Default fixture (from create_default_interleaved_pwm):            */
/*    time_period=10000, total_gpio=2 → slot=5000                     */
/*    dead_time=1000     → max pulse = 4000                           */
/* ================================================================== */

TEST_CASE("changeWidth: succeeds with valid width on channel 0",
          "[interleaved_pwm][changeWidth]")
{
    create_default_interleaved_pwm();
    
    int ret = PWM_SET_WIDTH(interleaved_pwm, 0, 3000);
    TEST_ASSERT_EQUAL(0,ret);
}

TEST_CASE("changeWidth: succeeds with valid width on channel 1",
          "[interleaved_pwm][changeWidth]")
{
    create_default_interleaved_pwm();
    int ret = PWM_SET_WIDTH(interleaved_pwm, 1, 1500);
    TEST_ASSERT_EQUAL(0,ret);
}

TEST_CASE("changeWidth: succeeds while PWM is running",
          "[interleaved_pwm][changeWidth]")
{
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, PWM_START(interleaved_pwm));
    int ret = PWM_SET_WIDTH(interleaved_pwm, 0, 3000);
    TEST_ASSERT_EQUAL(0,ret);
}

TEST_CASE("changeWidth: each channel can be changed independently",
          "[interleaved_pwm][changeWidth]")
{
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, PWM_SET_WIDTH(interleaved_pwm, 0, 1000));
    TEST_ASSERT_EQUAL(0, PWM_SET_WIDTH(interleaved_pwm, 1, 3500));
}

TEST_CASE("changeWidth: can be called multiple times on same channel",
          "[interleaved_pwm][changeWidth]")
{
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, PWM_SET_WIDTH(interleaved_pwm, 0, 1000));
    TEST_ASSERT_EQUAL(0, PWM_SET_WIDTH(interleaved_pwm, 0, 2000));
    TEST_ASSERT_EQUAL(0, PWM_SET_WIDTH(interleaved_pwm, 0, 3000));
}

/* --- Boundary conditions ----------------------------------------- */

TEST_CASE("changeWidth: pulse_width of 1 is minimum valid value",
          "[interleaved_pwm][changeWidth]")
{
    /* slot=5000, dead_time=1000 → 1+1000=1001 < 5000 ✓ */
    create_default_interleaved_pwm();
    int ret = PWM_SET_WIDTH(interleaved_pwm, 0, 1);
    TEST_ASSERT_EQUAL(0,ret);
}

TEST_CASE("changeWidth: pulse_width of 0 is accepted or rejected per spec",
          "[interleaved_pwm][changeWidth]")
{
    /*
     * 0-width means the channel is effectively off.
     * Decide whether your API treats this as a valid "disable" or an error.
     * CHOOSE ONE:
     */
    create_default_interleaved_pwm();
    int ret = PWM_SET_WIDTH(interleaved_pwm, 0, 0);
    TEST_ASSERT_EQUAL(0,ret);           /* tolerant: 0 = channel off */
    /* TEST_ASSERT_NULL(interleaved_pwm); */ /* strict:   must be > 0     */
}

TEST_CASE("changeWidth: pulse_width exactly at slot boundary is boundary condition",
          "[interleaved_pwm][changeWidth]")
{
    /*
     * slot=5000, dead_time=1000 → max = slot - dead_time = 4000
     * CHOOSE ONE to match your constraint (< or <=):
     */
    create_default_interleaved_pwm();
    int ret = PWM_SET_WIDTH(interleaved_pwm, 0, 4000);
    TEST_ASSERT_EQUAL(0,ret);           /* <= slot-dead_time allowed */
    /* TEST_ASSERT_NULL(interleaved_pwm); */ /* < slot-dead_time only     */
}

/* --- Rejection cases --------------------------------------------- */

TEST_CASE("changeWidth: fails when pulse_width plus dead_time exceeds slot",
          "[interleaved_pwm][changeWidth]")
{
    /* slot=5000, dead_time=1000 → 4500+1000=5500 > 5000 → FAIL */
    create_default_interleaved_pwm();
    int ret = PWM_SET_WIDTH(interleaved_pwm, 0, 4500);
    TEST_ASSERT_NOT_EQUAL(0,ret);
}

TEST_CASE("changeWidth: fails when pulse_width equals time_period",
          "[interleaved_pwm][changeWidth]")
{
    /* 10000 >> slot of 5000, must clearly fail */
    create_default_interleaved_pwm();
    int ret = PWM_SET_WIDTH(interleaved_pwm, 0, 10000);
    TEST_ASSERT_NOT_EQUAL(0,ret);
}

TEST_CASE("changeWidth: rejected width does not alter previous valid width",
          "[interleaved_pwm][changeWidth]")
{
    /*
     * After a failed changeWidth the channel must still be running
     * at its prior width. Verify by checking start/stop still work —
     * no internal state corruption.
     */
    create_default_interleaved_pwm();
    PWM_SET_WIDTH(interleaved_pwm, 0, 2000);
    TEST_ASSERT_EQUAL(0,    PWM_SET_WIDTH(interleaved_pwm, 0, 2000));
    TEST_ASSERT_NOT_EQUAL(0,PWM_SET_WIDTH(interleaved_pwm, 0, 9999));
    /* Component must still be usable */
    TEST_ASSERT_EQUAL(0, PWM_START(interleaved_pwm));
    TEST_ASSERT_EQUAL(0, PWM_STOP(interleaved_pwm));
}

/* --- Invalid arguments ------------------------------------------- */

TEST_CASE("changeWidth: fails when self is NULL",
          "[interleaved_pwm][changeWidth]")
{
    create_default_interleaved_pwm();
    
    int ret = interleaved_pwm->changePulseWidth(NULL, 0, 2000);
    TEST_ASSERT_NOT_EQUAL(0,ret);
}

TEST_CASE("changeWidth: fails when channel_no is out of range",
          "[interleaved_pwm][changeWidth]")
{
    /* total_gpio=2, so valid channels are 0 and 1 only */
    create_default_interleaved_pwm();
    int ret = PWM_SET_WIDTH(interleaved_pwm, 2, 2000);
    TEST_ASSERT_NOT_EQUAL(0,ret);
}

/* --- Manual / observable ----------------------------------------- */

TEST_CASE("manual: changeWidth duty change visible on oscilloscope",
          "[interleaved_pwm][changeWidth][manual]")
{
    /*
     * Observe GPIO 5 (channel 0) stepping through three duty levels:
     *   1 s at 10 % (1000 µs),  1 s at 20 % (2000 µs),  1 s at 40 % (4000 µs)
     * No glitches or missing pulses should appear between steps.
     */
    create_default_interleaved_pwm();
    TEST_ASSERT_EQUAL(0, PWM_START(interleaved_pwm));

    uint32_t widths[] = {1000, 2000, 4000};
    for (int i = 0; i < 3; i++)
    {
        ESP_LOGI(TAG, "changeWidth → %lu µs", widths[i]);
        TEST_ASSERT_EQUAL(0, PWM_SET_WIDTH(interleaved_pwm, 0, widths[i]));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    TEST_ASSERT_EQUAL(0, PWM_STOP(interleaved_pwm));
}



/* ================================================================== */
/*  GROUP 8 – Resolution and frequency validation                     */
/*                                                                     */
/*  Fixture: 4 channels, dead_time=1000, clk=80MHz                   */
/*  res_min  = ceil(log2(4/0.05)) = 7 bits                            */
/*  max pass = floor(log2(80000000/freq)) >= 7                        */
/*  boundary = 80000000 / 2^7 = 625 000 Hz                           */
/* ================================================================== */

/* --- Valid frequencies ------------------------------------------- */

TEST_CASE("resolution: low frequency 50 Hz is accepted",
          "[interleaved_pwm][resolution]")
{
    /* res_max = floor(log2(80000000/50)) = floor(20.6) = 20 → capped at chip max */
    static uint8_t  gpio_list[4]    = {5, 18, 22, 23};
    static uint32_t pulse_widths[4] = {2000, 2000, 2000, 2000};

    interleaved_pwm_config_t config = {
        .gpio_no      = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio   = 4,
        .dead_time    = 1000,
        .time_period    = 1000000/50            //Timer period in microseconds
    };

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NOT_NULL(interleaved_pwm);
    interleavedPWMCreated = true;
}

TEST_CASE("resolution: mid frequency 1 kHz is accepted",
          "[interleaved_pwm][resolution]")
{
    /* res_max = floor(log2(80000000/1000)) = floor(16.3) = 16 > 7 */
    static uint8_t  gpio_list[4]    = {5, 18, 22, 23};
    static uint32_t pulse_widths[4] = {200, 200, 200, 200};

    interleaved_pwm_config_t config = {
        .gpio_no      = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio   = 4,
        .dead_time    = 45,
        .time_period    = 1000000/1000      //time period in us
    };

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NOT_NULL(interleaved_pwm);
    interleavedPWMCreated = true;
}

TEST_CASE("resolution: 100 kHz is accepted",
          "[interleaved_pwm][resolution]")
{
    /* res_max = floor(log2(80000000/100000)) = floor(9.6) = 9 > 7 */
    static uint8_t  gpio_list[4]    = {5, 18, 22, 23};
    static uint32_t pulse_widths[4] = {1, 1, 1, 1};

    interleaved_pwm_config_t config = {
        .gpio_no      = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio   = 4,
        .dead_time    = 1,
        .time_period = 1000000/100000
    };

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NOT_NULL(interleaved_pwm);
    interleavedPWMCreated = true;
}

/* --- Boundary ---------------------------------------------------- */

TEST_CASE("resolution: 625 kHz is the boundary frequency for 4 channels",
          "[interleaved_pwm][resolution]")
{
    /*
     * res_max = floor(log2(80000000/625000)) = floor(log2(128)) = 7
     * res_min = 7
     * res_max == res_min → exactly passes
     */
    static uint8_t  gpio_list[4]    = {5, 18, 22, 23};
    static uint32_t pulse_widths[4] = {1, 1, 1, 1};

    interleaved_pwm_config_t config = {
        .gpio_no      = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio   = 4,
        .dead_time    = 0,
        .time_period = 1000000/625000
    };

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NOT_NULL(interleaved_pwm);
    interleavedPWMCreated = true;
}

TEST_CASE("resolution: one step above boundary 626 kHz is rejected",
          "[interleaved_pwm][resolution]")
{
    /*
     * res_max = floor(log2(80000000/626000)) = floor(log2(127.8)) = 6
     * res_min = 7
     * res_max < res_min → rejected
     */
    static uint8_t  gpio_list[4]    = {5, 18, 22, 23};
    static uint32_t pulse_widths[4] = {1, 1, 1, 1};

    interleaved_pwm_config_t config = {
        .gpio_no      = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio   = 4,
        .dead_time    = 0,
        .time_period = 1000000/626000
    };

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NULL(interleaved_pwm);
}

/* --- Rejection cases --------------------------------------------- */

TEST_CASE("resolution: 1 MHz is rejected for 4 channels",
          "[interleaved_pwm][resolution]")
{
    /* res_max = floor(log2(80)) = 6 < res_min(7) → rejected */
    static uint8_t  gpio_list[4]    = {5, 18, 22, 23};
    static uint32_t pulse_widths[4] = {1, 1, 1, 1};

    interleaved_pwm_config_t config = {
        .gpio_no      = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio   = 4,
        .dead_time    = 0,
        .time_period = 1000000/1000000
    };

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NULL(interleaved_pwm);
}

TEST_CASE("resolution: time_period of zero is rejected",
          "[interleaved_pwm][resolution]")
{
    /* log2(clk/0) is undefined — must not crash or divide by zero */
    static uint8_t  gpio_list[4]    = {5, 18, 22, 23};
    static uint32_t pulse_widths[4] = {2000, 2000, 2000, 2000};

    interleaved_pwm_config_t config = {
        .gpio_no      = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio   = 4,
        .dead_time    = 1000,
        .time_period = 0
    };

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NULL(interleaved_pwm);
}

/* --- Channel count affects boundary ------------------------------ */

TEST_CASE("resolution: fewer channels allows higher frequency",
          "[interleaved_pwm][resolution]")
{
    /*
     * With 2 channels:
     *   res_min = ceil(log2(2/0.05)) = ceil(log2(40)) = ceil(5.32) = 6 bits
     *   boundary = 80000000 / 2^6 = 1 250 000 Hz
     *   So 1 MHz should pass for 2 channels but fails for 4.
     */
    static uint8_t  gpio_list[2]    = {5, 18};
    static uint32_t pulse_widths[2] = {1, 1};

    interleaved_pwm_config_t config = {
        .gpio_no      = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio   = 2,
        .dead_time    = 0,
        .time_period = 1000000/1000000
    };

    esp_err_t ret=interleavedPWMCreate(&config,&interleaved_pwm);
    TEST_ASSERT_NOT_NULL(interleaved_pwm);
    interleavedPWMCreated = true;
}