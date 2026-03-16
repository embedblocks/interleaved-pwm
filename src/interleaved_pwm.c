/*There is no check to have exclusive pwm channel assignemnt for multiple instances
If multiple instances of pwm manager are to be created, they will still use use  from 
channel number 0 to total channel required. Actually a single channel can be associated with
multiple gpio in esp32, so all those gpio will have same pwm signal.
*/

#include "esp_log.h"
#include "esp_err.h"
#include "pwm_line.h"
#include "interleaved_pwm.h"


//static const uint8_t duty_one=100;
//static const uint8_t duty_two=100;



static const char* TAG="interleaved pwm";




#define CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))





#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})




//not used
static int proberCheckDeadTime(uint32_t time_period,uint32_t dead_time){
    uint32_t percentage=(dead_time*100)/time_period;

    //Greater than 5%
    if(percentage>5)    
        return ERR_PROBE_MANAGER_DEAD_TIME_LARGE;
    //if(percentage<)
    return 0;
}


static int frequencyCheck(uint32_t frequency){

    if(frequency>100)   //100Hz because it gives  10ms. less than 10ms is less time for 4 input keypad
        return ERR_PROBE_MANAGER_FREQUENCY_LARGE;

    return 0;
}

static int phaseCalculate(uint8_t total_gpio){
    if(total_gpio<=0)
        return ERR_PROBE_MANAGER_WRONG_PARAMETERS; 
    return 360/total_gpio;
}

/// @brief  Allm channles get slot equally divided from total time_period
////////The pulse width + the required dead time before next pulse starts must not exceed the slot
/// @param pulse_widths 
/// @param dead_time 
/// @param slot_width 
/// @return 
static bool pulseWidthInvalid(uint32_t pulse_widths,
                           uint32_t dead_time,
                           uint32_t slot_width){

    if((pulse_widths + dead_time) > slot_width)
        return true;

    return false;

}

static int pulseWidthCheck(uint32_t* pulse_widths,
                           uint8_t total_gpio,
                           uint32_t dead_time,
                           uint32_t time_period)
{
    if(total_gpio == 0)
        return ERR_PROBE_MANAGER_WRONG_PARAMETERS;

    uint32_t slot = time_period / total_gpio;

    for(uint8_t i = 0; i < total_gpio; i++)
    {
        //If width + dead time exceeds the slot 
        if(pulseWidthInvalid(pulse_widths[i],dead_time,slot))
            return ERR_PROBE_MANAGER_WRONG_PARAMETERS;
    }

    return 0;
}




static int start(interleaved_pwm_interface_t* self){

    if(self==NULL)    
        return ESP_FAIL;

    interleaved_pwm_t* prb=container_of(self,interleaved_pwm_t,interface);

    pwm_line_t* lines=(pwm_line_t*) prb->lines;
    if(lines==NULL)
        return ESP_FAIL;
    uint8_t total_lines=prb->total_lines;

    for(uint8_t i=0;i<total_lines;i++){
        lines[i].interface.pwmStart(&lines[i].interface);
    }

    return 0;
}


static int stop(interleaved_pwm_interface_t* self){

    if(self==NULL)    
        return ESP_FAIL;

    interleaved_pwm_t* prb=container_of(self,interleaved_pwm_t,interface);

    pwm_line_t* lines=(pwm_line_t*) prb->lines;
    if(lines==NULL)
        return ESP_FAIL;
    uint8_t total_lines=prb->total_lines;

    for(uint8_t i=0;i<total_lines;i++){
        lines[i].interface.pwmStop(&lines[i].interface);
    }

    return 0;
}


static int changeWidth(interleaved_pwm_interface_t* self,uint8_t channel_no,uint32_t pulse_width){

    
    if(self==NULL)    
        return ESP_FAIL;
    interleaved_pwm_t* prb=container_of(self,interleaved_pwm_t,interface);

    pwm_line_t* lines=(pwm_line_t*) prb->lines;

    if(lines==NULL)
        return ESP_FAIL;
    uint8_t total_lines=prb->total_lines;

    //Check channel_no>(total_lines-1) because channel number s start from 0
    if(channel_no<0 || channel_no>(total_lines-1))
        return ESP_FAIL;

    ESP_LOGI(TAG,"changing total_lines %d  slot width %d, channel_no %d",total_lines,prb->time_period/prb->total_lines,channel_no);
    if(pulseWidthInvalid(pulse_width,prb->dead_time,prb->time_period/prb->total_lines))
        return ESP_FAIL;

    lines[channel_no].interface.pwmChangeWidth(&lines[channel_no].interface,pulse_width,prb->time_period);
    //for(uint8_t i=0;i<total_lines;i++){
      //  lines[i].interface.pwmStop(&lines[i].interface);
    //}

    return 0;
}


static int destroy(interleaved_pwm_interface_t* self)
{
    if(self==NULL)    
        return ESP_FAIL;
    interleaved_pwm_t* prb = container_of(self, interleaved_pwm_t, interface);

    pwm_line_t* lines = prb->lines;
    if(lines==NULL)
        return ESP_FAIL;

    ESP_LOGI(TAG,"destroying prober, total lines %d",prb->total_lines);

    for(uint8_t i=0;i<prb->total_lines;i++)
    {
        lines[i].interface.pwmDestroy(&lines[i].interface);
    }

    free(lines);
    prb->lines = NULL;

    return 0;
}

int interleavedPWMCreate(interleaved_pwm_t* self, interleaved_pwm_config_t* config)
{
    if ( self == NULL ||
         config == NULL ||
         config->gpio_no == NULL ||
         config->pulse_widths == NULL ||
         config->time_period == 0 ||
         config->total_gpio == 0 )
    {
        return ERR_PROBE_MANAGER_INVALID_MEM;
    }

    uint8_t total_gpio   = config->total_gpio;
    uint32_t time_period = config->time_period;
    uint32_t dead_time   = config->dead_time;
    uint32_t* pulse_widths = config->pulse_widths;
    uint8_t* gpio_no       = config->gpio_no;

    int frequency = 1000000 / time_period;

    /* Check pulse widths against slot limits */
    int ret = pulseWidthCheck(pulse_widths, total_gpio, dead_time, time_period);
    if(ret != 0)
        return ret;

    //ret = frequencyCheck(frequency);
    //if(ret != 0)
      //  return ret;

    /* Calculate slot time for interleaving */
    //uint32_t slot = time_period / total_gpio;
    int phase= phaseCalculate(config->total_gpio);

    pwm_line_t* pwm_line = malloc(sizeof(pwm_line_t) * total_gpio);
    if(pwm_line == NULL)
        return ESP_ERR_NO_MEM;

    self->lines = pwm_line;
    self->total_lines=total_gpio;

    pwm_config_t line_config;
    uint32_t current_phase = 0;

    for(uint8_t i = 0; i < total_gpio; i++)
    {
        line_config.pulse_width    = pulse_widths[i];
        line_config.channel_number = i;
        line_config.dead_time      = 0;         //Not used in new design, bcz becomes meaningless at steady state, all get a deadtime offset
        line_config.gpio           = gpio_no[i];
        line_config.phase          = current_phase;
        line_config.time_period    = time_period;

        ESP_ERROR_CHECK(pwmCreate(&pwm_line[i], &line_config));

        current_phase += phase;

        ESP_LOGI(TAG, "creating channel %d phase %lu", i, current_phase);
    }

    self->interface.start   = start;
    self->interface.stop    = stop;
    self->interface.destroy = destroy;
    self->interface.changePulseWidth=changeWidth;

    self->time_period = time_period;
    self->dead_time=dead_time;

    ESP_LOGI(TAG, "interleaved PWM created");

    return 0;
}
