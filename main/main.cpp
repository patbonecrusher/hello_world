#include <stdio.h>                   // printf()
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"           // for task and timing
#include "driver/ledc.h"

#define LEDC_DEFAULT_CLK         0

unsigned long IRAM_ATTR millis()
{
    return (unsigned long) (esp_timer_get_time() / 1000ULL);
}
class LedChannel {
public:
    LedChannel(uint8_t chan, uint8_t pin)
    : chan(chan), pin(pin), freq(5000), resolution(0), group(0), timer(0) {
    }

    bool Setup(double freq, uint8_t resolution) {
        this->freq = freq;
        this->resolution = resolution;
        this->group=(this->chan/8);
        this->timer=((this->chan/2)%4);

        ledc_timer_config_t ledc_timer = {
            .speed_mode       = (ledc_mode_t) this->group,
            .duty_resolution  = (ledc_timer_bit_t) this->resolution,
            .timer_num        = (ledc_timer_t) this->timer,
            .freq_hz          = (uint32_t) this->freq,
            .clk_cfg          = (ledc_clk_cfg_t) LEDC_DEFAULT_CLK
        };

        ledc_timer_config(&ledc_timer);

        uint8_t channel=(this->chan%8);

        ledc_channel_config_t ledc_channel = {
            .gpio_num       = (int) this->pin,
            .speed_mode     = (ledc_mode_t) this->group,
            .channel        = (ledc_channel_t) channel,
            .intr_type      = (ledc_intr_type_t) LEDC_INTR_DISABLE,
            .timer_sel      = (ledc_timer_t) this->timer,
            .duty           = 0,
            .hpoint         = 0,
            .flags = {
                .output_invert = 0
            }
        };
        ledc_channel_config(&ledc_channel);

        return true;
    }

    bool SetDuty(uint32_t duty) {
        uint32_t max_duty = (1 << this->resolution) - 1;
        printf("%p\n", this);

        if(duty == max_duty){
            duty = max_duty + 1;
        }

        uint8_t channel=(chan%8);

        ledc_set_duty((ledc_mode_t) this->group, (ledc_channel_t) channel, duty);
        ledc_update_duty((ledc_mode_t) this->group, (ledc_channel_t) channel);
        return true;
    }

private:
    uint8_t chan;
    uint8_t pin;
    double freq;
    uint8_t resolution;
    uint8_t group;
    uint8_t timer;

};

// Using static polymorphism through the use of the C++ CRTP pattern to:
//  * Reduces memory footprint no vtable since nothing is virtual
// Read more on the topic:
//      https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
//      https://fjrg76.wordpress.com/2018/05/23/objectifying-task-creation-in-freertos-ii/
//      https://www.fluentcpp.com/2017/05/16/what-the-crtp-brings-to-code/
template<typename T>
class ThreadX {
public:
    ThreadX()
    {
    }

    void Start(unsigned _stackDepth, UBaseType_t _priority, const char* _name = "" ) {
        xTaskCreate( task, _name, _stackDepth, this, _priority, &this->taskHandle);
    }

    TaskHandle_t GetHandle()
    {
        return this->taskHandle;
    }

    void Main()
    {
        static_cast<T&>(*this).Main();
    }

private:
    static void task( void* _params )
    {
        ThreadX* p = static_cast<ThreadX*>( _params );
        p->Main();
    }

    TaskHandle_t taskHandle;
};

class LedFaderX : public ThreadX<LedFaderX>
{
public:
    LedFaderX(unsigned _stackDepth, UBaseType_t _priority, const char* _name, LedChannel *_ledChannel, uint32_t _ticks = 1 )
    : ledChannel(_ledChannel)
    , ticks( _ticks )
    {
        this->Start(_stackDepth, _priority, _name);
    }

    void Main()
    {
        float val = 0;
        while( 1 )
        {
            val = (exp(sin(millis()/2000.0*M_PI)) - 0.36787944)*108.0;
            printf("%f\n", val);
            this->ledChannel->SetDuty((uint32_t) val);
            vTaskDelay(this->ticks);
        }
    }
private:
    LedChannel* ledChannel;
    uint32_t ticks;
};



void ledc_fade(void* params) {
    LedChannel* led = static_cast<LedChannel*>( params );
	
    float val;
	for(;;) {
		val = (exp(sin(millis()/2000.0*M_PI)) - 0.36787944)*108.0;
		printf("%f\n", val);
        led->SetDuty((uint32_t) val);
		vTaskDelay(1);
	}
}

extern "C" void app_main()
{
    printf("Hello Breathe LEDC!\n");

    LedChannel *led1 = new LedChannel(0, 5);
    led1->Setup(5000, 8);
    printf("%p\n", led1);

    // printf("Start LEDC Breathing\n");
    // xTaskCreate(ledc_fade, "ledc_fade", 24*configMINIMAL_STACK_SIZE, &led1, 2, NULL);
    LedFaderX lf{ 8196, 2, "ledx", led1, 1 };
    printf("LEDC Task running\n");
}
