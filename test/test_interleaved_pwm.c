#include "unity.h"
#include "interleaved_pwm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"


static const char* TAG = "test_interleaved_pwm";
static interleaved_pwm_t interleaved_pwm;
static bool interleavedPWMCreated = false;


/*-----------------------------------------------------------
                    Unity hooks
-----------------------------------------------------------*/

void setUp(void)
{
    interleavedPWMCreated = false;
}

void tearDown(void)
{
    if(interleavedPWMCreated)
    {
        ESP_LOGI(TAG,"Tearing down interleaved_pwm");
        interleaved_pwm.interface.destroy(&interleaved_pwm.interface);
        interleavedPWMCreated = false;
    }
}


/*-----------------------------------------------------------
                Helper test configuration
-----------------------------------------------------------*/

static void create_default_interleaved_pwm(void)
{
    static uint8_t gpio_list[2] = {5,18};
    static uint32_t pulse_widths[2] = {2000,2000};

    interleaved_pwm_config_t config = {
        .gpio_no = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio = 2,
        .dead_time = 1000,
        .time_period = 10000
    };

    int ret = interleavedPWMCreate(&interleaved_pwm,&config);
    ESP_LOGI(TAG,"return %d",ret);

    TEST_ASSERT_EQUAL(0,ret);

    interleavedPWMCreated = true;
}


/*-----------------------------------------------------------
                        Tests
-----------------------------------------------------------*/


TEST_CASE("interleavedPWMCreate succeeds with valid configuration", "[interleaved_pwm]")
{
    create_default_interleaved_pwm();
}


TEST_CASE("interleavedPWMCreate fails when config is NULL", "[interleaved_pwm]")
{
    int ret = interleavedPWMCreate(&interleaved_pwm,NULL);

    TEST_ASSERT_NOT_EQUAL(0,ret);
}


TEST_CASE("interleaved_pwm start works", "[interleaved_pwm]")
{
    create_default_interleaved_pwm();

    int ret = interleaved_pwm.interface.start(&interleaved_pwm.interface);

    TEST_ASSERT_EQUAL(0,ret);
}


TEST_CASE("interleaved_pwm stop works", "[interleaved_pwm]")
{
    create_default_interleaved_pwm();

    TEST_ASSERT_EQUAL(0,interleaved_pwm.interface.start(&interleaved_pwm.interface));

    int ret = interleaved_pwm.interface.stop(&interleaved_pwm.interface);

    TEST_ASSERT_EQUAL(0,ret);
}


TEST_CASE("interleaved_pwm destroy releases resources", "[interleaved_pwm]")
{
    create_default_interleaved_pwm();

    int ret = interleaved_pwm.interface.destroy(&interleaved_pwm.interface);

    TEST_ASSERT_EQUAL(0,ret);

    interleavedPWMCreated = false;
}


TEST_CASE("multiple pwm lines can be created", "[interleaved_pwm]")
{
    static uint8_t gpio_list[4] = {5,18,22,23};
    static uint32_t pulse_widths[4] = {2000,2000,2000,2000};

    interleaved_pwm_config_t config = {
        .gpio_no = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio = 4,
        .dead_time = 1000,
        .time_period = 15000
    };

    int ret = interleavedPWMCreate(&interleaved_pwm,&config);

    TEST_ASSERT_EQUAL(0,ret);

    interleavedPWMCreated = true;
}


TEST_CASE("pulse width exceeding period should fail", "[interleaved_pwm]")
{
    static uint8_t gpio_list[2] = {5,18};
    static uint32_t pulse_widths[2] = {6000,6000};

    interleaved_pwm_config_t config = {
        .gpio_no = gpio_list,
        .pulse_widths = pulse_widths,
        .total_gpio = 2,
        .dead_time = 2000,
        .time_period = 10000
    };

    int ret = interleavedPWMCreate(&interleaved_pwm,&config);

    TEST_ASSERT_NOT_EQUAL(0,ret);
}


TEST_CASE("pwm waveform observable test", "[interleaved_pwm][manual]")
{
    create_default_interleaved_pwm();

    TEST_ASSERT_EQUAL(0,interleaved_pwm.interface.start(&interleaved_pwm.interface));

    /* Observe waveform with oscilloscope or logic analyzer */
    vTaskDelay(pdMS_TO_TICKS(3000));

    TEST_ASSERT_EQUAL(0,interleaved_pwm.interface.stop(&interleaved_pwm.interface));
}