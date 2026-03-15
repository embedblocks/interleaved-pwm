#ifndef PROBE_MANAGER_H
#define PROBE_MANAGER_H
#include <stdint.h>
//#include "pwm_line.h"


#define         ERR_PROBE_MANAGER_BASE                          0
#define         ERR_PROBE_MANAGER_INVALID_MEM                   (ERR_PROBE_MANAGER_BASE-1)
#define         ERR_PROBE_MANAGER_WRONG_PARAMETERS              (ERR_PROBE_MANAGER_BASE-2)
#define         ERR_PROBE_MANAGER_DEAD_TIME_LARGE               (ERR_PROBE_MANAGER_BASE-3)
#define         ERR_PROBE_MANAGER_FREQUENCY_LARGE               (ERR_PROBE_MANAGER_BASE-4)





/*The parameters are not in same units. For example time period is in microseconds where as dutycycle
is given in percentage instead of microseconds. So values in accepted in format which are convinient
to the enduser. And it is the job of the class to calculate/convert the values.
*/
typedef struct{

    uint8_t* gpio_no;
    uint32_t* pulse_widths;          //in microseconds. Array size equal to total_gpio. Width of each pulse. cannot be caluculated as complex requireent of distinct.
    uint8_t total_gpio;
    uint32_t time_period;           //in microseconds
    uint32_t dead_time;             //microseconds. Dead Time between each pulse. The phase is determined by this
}interleaved_pwm_config_t;



typedef struct {
    int (*start)(struct interleaved_pwm_interface* self);
    int (*stop)(struct interleaved_pwm_interface* self);
    int (*destroy)(struct interleaved_pwm_interface* self);

    uint32_t (*getTimePeriod)();
}interleaved_pwm_interface_t;


//It needs to be changed. This  whole struct must be private and only interface must be returned

typedef struct {
    uint32_t time_period;
    void* lines;                //internally typcasted to  pwm_line_t. encapsulation
    uint8_t total_lines;
    interleaved_pwm_interface_t interface;
}interleaved_pwm_t;   


int proberCreate(interleaved_pwm_t* self,interleaved_pwm_config_t* config);
#endif