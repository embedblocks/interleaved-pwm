
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "pwm_line.h"


void app_main(void)
{

    pwm_line_t line1;
    //gpio_confog
//    ESP_LOGI(TAG, "Setting up test environment...");
    pwm_config_t config1={.frequency=100,
                        .duty_cycle=25,
                        .phase=0,
                        .phase_cut_on_duty=20,
                        .gpio=5,
                        .channel_number=0,
                        };

    pwmCreate(&line1,&config1);

    while(1){

        vTaskDelay(pdMS_TO_TICKS(1));
        printf("\n %d",gpio_get_level(5));

    }


}
