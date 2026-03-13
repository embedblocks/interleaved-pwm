idf_component_register(SRCS "pwm-main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES pwm-line esp_driver_gpio)
