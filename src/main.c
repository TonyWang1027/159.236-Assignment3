/* Assignment 3 for 159.236 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <esp_adc_cal.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <nvs_flash.h>

#include "fonts.h"
#include "graphics.h"

struct Clock
{
    int hour;
    int minute;
    int second;
};

struct MenuSelectionBox
{
    int x_pos;
    int y_pos;
    int height;
    int weight;
    int status;
};

struct Clock my_clock;
struct MenuSelectionBox menuSelectionBox;

QueueHandle_t inputQueue;
uint16_t lastkeytime = 0;
int buttonPressDuration = 0;

// interrupt handler for button presses on GPIO0 and GPIO35
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    static int pval[2]={1,1};
    uint32_t gpio_num = (uint32_t) arg;
    int gpio_index=(gpio_num==35);
    int val=(1-pval[gpio_index]);
    
    uint64_t time=esp_timer_get_time();
    uint64_t timesince=time-lastkeytime;
    //ets_printf("gpio_isr_handler %d %d %lld\n",gpio_num,val, timesince);
    // the buttons can be very bouncy so debounce by checking that it's been .5ms since the last
    // change and that it's pressed down
    if(timesince>500 && val == 0) {
        xQueueSendFromISR(inputQueue,&gpio_num,0); 
        
    }
    pval[gpio_index]=val;
    lastkeytime=time;   
    gpio_set_intr_type(gpio_num,val==0?GPIO_INTR_HIGH_LEVEL:GPIO_INTR_LOW_LEVEL);
    
}

void init_menuSelectionBox()
{
    menuSelectionBox.x_pos = 10;
    menuSelectionBox.y_pos = 0;
    menuSelectionBox.height = 13;
    menuSelectionBox.weight = 107;
    menuSelectionBox.status = 0;
}

void set_current_time()
{
    struct tm *local;
    time_t t;
    t = time(NULL);
    local = localtime(&t);

    my_clock.hour = 0;
    my_clock.minute = 0;
    my_clock.second = 0;

    // printf("%02d:%02d:%02d\n", local->tm_hour, local->tm_min, local->tm_sec);
}

void menuStatusChange()
{
    if(menuSelectionBox.x_pos == 10)
    {
        menuSelectionBox.x_pos = 125;
        menuSelectionBox.status = 2;
    }
    else
    {
        menuSelectionBox.x_pos = 10;
        menuSelectionBox.status = 1;
    }
}

void increamentTime()
{
    my_clock.second++;
    if(my_clock.second == 60)
    {
        my_clock.minute++;
        my_clock.second = 0;
    }
    if(my_clock.minute == 60)
    {
        my_clock.hour++;
        my_clock.minute = 0;
    }
    if(my_clock.hour == 24)
    {
        my_clock.hour = 0;
    }
}

void main_screen()
{
    set_current_time();
    init_menuSelectionBox();

    /* Variable Declaration */
    int64_t current_time = 0;
    int64_t last_time = 0;

    char buffer[20];

    while (true)
    {
        last_time = esp_timer_get_time() / (int64_t) 1000000;
        cls(rgbToColour(0, 0, 0));
        setFontColour(255, 255, 255);

        /* Anything need to draw on the screen should write here 00:00:00*/
        /*** START ***/
        setFont(FONT_DEJAVU24);
        snprintf(buffer, 20, "%02d:%02d:%02d", my_clock.hour, my_clock.minute, my_clock.second);
        print_xy(buffer, (display_width / 2) - 55, (display_height / 2) - 10);

        // plus 115 to move right
        draw_rectangle(menuSelectionBox.x_pos, menuSelectionBox.y_pos, menuSelectionBox.weight, menuSelectionBox.height, rgbToColour(0, 172, 13));

        setFont(FONT_SMALL);
        snprintf(buffer, 20, "Set Current Time");
        print_xy(buffer, 15, 1);
        snprintf(buffer, 20, "Set Alarm Time");
        print_xy(buffer, 135, 1);
        
        /*** END!!! ***/

        send_frame();
        wait_frame();

        // bottom(left) button pressed event
        if(gpio_get_level(0) == 0)
        {
            if(buttonPressDuration == 0) menuStatusChange();
            if(buttonPressDuration <= 5) buttonPressDuration++;
        }
        else
        {
            if(buttonPressDuration != 0) buttonPressDuration = 0;
        }

        // top(right) button pressed event
        if(gpio_get_level(35) == 0)
        {
            
        }
        
        current_time = esp_timer_get_time() / (int64_t) 1000000;

        if(current_time != last_time)
        {
            // printf("current time: %lld\n", current_time);
            increamentTime();
        }
    }
    
}

void app_main()
{
    inputQueue = xQueueCreate(4, 4);
    ESP_ERROR_CHECK(nvs_flash_init());

    gpio_set_direction(0, GPIO_MODE_INPUT);
    gpio_set_direction(35, GPIO_MODE_INPUT);
    gpio_set_intr_type(0, GPIO_INTR_LOW_LEVEL);
    gpio_set_intr_type(35, GPIO_INTR_LOW_LEVEL);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(0, gpio_isr_handler, (void*) 0);
    gpio_isr_handler_add(35, gpio_isr_handler, (void*) 35);

    graphics_init();
    cls(0);

    set_orientation(0);

    main_screen();
}
