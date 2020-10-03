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
    uint8_t isAm;
};

struct SelectionBox
{
    int x_pos;
    int y_pos;
    int height;
    int weight;
    int status;
};

struct Clock myClock;
struct Clock myAlarmClock;
struct SelectionBox menuSelectionBox;
struct SelectionBox digitSelectionBox;
int digitPosition[4];

QueueHandle_t inputQueue;
uint16_t lastkeytime = 0;
int buttonPressDuration = 0;
int clockMode = 0;
// mode: 0 => in main screen
// mode: 1 => in set current time mode
// mode: 2 => in set alarm time mode

char buffer[20];

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

void initTime()
{
    // struct tm *local;
    // time_t t;
    // t = time(NULL);
    // local = localtime(&t);

    myClock.hour = myAlarmClock.hour = 0;
    myClock.minute = myAlarmClock.minute = 0;
    myClock.second = myAlarmClock.second = 0;
    myClock.isAm = myAlarmClock.isAm = 1;

    // printf("%02d:%02d:%02d\n", local->tm_hour, local->tm_min, local->tm_sec);
}

void init_digitSelectionBox()
{
    digitPosition[0] = 65;
    digitPosition[1] = 81;
    digitPosition[2] = 106;
    digitPosition[3] = 122;

    digitSelectionBox.x_pos = digitPosition[0];
    digitSelectionBox.y_pos = 55;
    digitSelectionBox.weight = 16;
    digitSelectionBox.height = 24;
    digitSelectionBox.status = 0;
}

void menuStatusChange()
{
    if(menuSelectionBox.x_pos == 10)
    {
        // Move to right option
        menuSelectionBox.x_pos = 125;
        menuSelectionBox.status = 1;
    }
    else
    {
        // Move to left option
        menuSelectionBox.x_pos = 10;
        menuSelectionBox.status = 0;
    }
}

void incrementTime()
{
    myClock.second++;
}

void clockLogic()
{
    if(myClock.second == 60)
    {
        myClock.minute++;
        myClock.second = 0;
    }
    if(myClock.minute == 60)
    {
        myClock.hour++;
        myClock.minute = 0;
    }
    if(myClock.hour == 12 && myClock.minute == 0 && myClock.second == 0 && myClock.isAm == 1)  // 12:00:00am
    {
        myClock.isAm = 0;
    }
    else if(myClock.hour == 12 && myClock.minute == 0 && myClock.second == 0 && myClock.isAm == 0)  // 12:00:00pm
    {
        myClock.isAm = 1;
        myClock.hour = 0;
    }
    else if(myClock.hour == 13 && myClock.minute == 0 && myClock.minute == 0 && myClock.isAm == 0)  // 13:00:00pm
    {
        myClock.hour = 1;
    }
}

void twelveHourClock()
{
    setFont(FONT_UBUNTU16);
    if(myClock.isAm == 1)
    {
        print_xy("AM", 185, 58);
    }
    else
    {
        print_xy("PM", 185, 58);
    }
}

void showCurrentTime()
{
    setFont(FONT_DEJAVU24);
    snprintf(buffer, 20, "%02d:%02d:%02d", myClock.hour, myClock.minute, myClock.second);
    print_xy(buffer, (display_width / 2) - 55, (display_height / 2) - 10);

    twelveHourClock();
}

void showAlarmTime()
{
    setFont(FONT_UBUNTU16);
    print_xy("Alarm Time:", 40, (display_height - 15));
    snprintf(buffer, 20, "%02d:%02d", myAlarmClock.hour, myAlarmClock.minute);
    print_xy(buffer, 142, (display_height - 15));

    // Bottom separate line
    draw_rectangle(0, (display_height - 17), display_width, 1, rgbToColour(255, 255, 255));
}

void showMenu()
{
    draw_rectangle(menuSelectionBox.x_pos, menuSelectionBox.y_pos, menuSelectionBox.weight, menuSelectionBox.height, rgbToColour(0, 172, 13));

    setFont(FONT_SMALL);
    snprintf(buffer, 20, "Set Current Time");
    print_xy(buffer, 15, 1);
    snprintf(buffer, 20, "Set Alarm Time");
    print_xy(buffer, 135, 1);

    // Top separate line
    draw_rectangle(0, 14, display_width, 1, rgbToColour(255, 255, 255));
}

void setCurrentTimeMode()
{
    init_digitSelectionBox();

    int8_t flashing = 0;
    int64_t current_time = 0;
    int64_t last_time = 0;

    int digitNumber = 0;

    vTaskDelay(10);

    while (1)
    {
        last_time = esp_timer_get_time() / (int64_t) 1000000;
        cls(rgbToColour(0, 0, 0));
        setFontColour(255, 255, 255);

        /* Anything need to draw on the screen should write here 00:00:00*/
        /*** START ***/
        digitSelectionBox.x_pos = digitPosition[digitNumber];

        if(flashing == 0)
        {
            draw_rectangle(digitSelectionBox.x_pos, digitSelectionBox.y_pos, digitSelectionBox.weight, digitSelectionBox.height, rgbToColour(98, 102, 109));
        }
        showCurrentTime();
        // showAlarmTime();

        setFont(FONT_UBUNTU16);
        print_xy("Set Current Time Mode", 1, 1);

        print_xy("OK", 205, 115);
        // showMenu();
        
        /*** END!!! ***/

        send_frame();
        wait_frame();

        // bottom(left) button pressed event
        if(gpio_get_level(0) == 0)
        {
            // this can be active when button is pressed, and only active once
            // if(buttonPressDuration == 0)
            // if(buttonPressDuration <= 5) buttonPressDuration++;
        }
        else
        {
            // if(buttonPressDuration != 0) buttonPressDuration = 0;
        }

        // top(right) button pressed event
        if(gpio_get_level(35) == 0)
        {
            if(buttonPressDuration == 0)
            {
                if(digitNumber < 3)
                {
                    digitNumber++;
                }
                else
                {
                    digitNumber = 0;
                }
            }
            if(buttonPressDuration <= 5) buttonPressDuration++;
        }
        else
        {
            if(buttonPressDuration != 0) buttonPressDuration = 0;
        }
        

        current_time = esp_timer_get_time() / (int64_t) 1000000;

        if(current_time != last_time)
        {
            // printf("current time: %lld\n", current_time);
            clockLogic();

            if(flashing == 0)
            {
                flashing = 1;
            }
            else
            {
                flashing = 0;
            }
        }
    }
    
}

void main_screen()
{
    initTime();
    init_menuSelectionBox();

    /* Variable Declaration */
    int64_t current_time = 0;
    int64_t last_time = 0;

    while (1)
    {
        last_time = esp_timer_get_time() / (int64_t) 1000000;
        cls(rgbToColour(0, 0, 0));
        setFontColour(255, 255, 255);

        /* Anything need to draw on the screen should write here 00:00:00*/
        /*** START ***/
        showCurrentTime();
        showAlarmTime();
        showMenu();
        
        /*** END!!! ***/

        send_frame();
        wait_frame();

        // bottom(left) button pressed event
        if(gpio_get_level(0) == 0)
        {
            // this can be active when button is pressed, and only active once
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
            if(menuSelectionBox.status == 0)  // set current time mode
            {
                printf("Left Box\n");
                clockMode = 1;
                setCurrentTimeMode();
            }
            else  // set alarm time mode
            {
                printf("Right Box\n");
            }
            
        }
        
        current_time = esp_timer_get_time() / (int64_t) 1000000;

        if(current_time != last_time)
        {
            // printf("current time: %lld\n", current_time);
            if(clockMode == 0) incrementTime();
            clockLogic();
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
