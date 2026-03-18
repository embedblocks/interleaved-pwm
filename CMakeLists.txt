file(GLOB_RECURSE MULTIPWM_SRCS
    src/*.c
    private/*.c
)

idf_component_register(
    SRCS ${MULTIPWM_SRCS}
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS
        private/pwm-line/
        private/pwm-line-interface/
    PRIV_REQUIRES esp_driver_ledc esp_driver_gpio)