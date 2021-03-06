/* Assignment 3 for 159.236 */

/* Option 1 - Description
Notice:
1. Once in the "Set Current Time" mode, the seconds digits will reset to zero.
2. When program first start, the current time and alarm time will all set to zero, but the alarm will not work until current time equal alarm time again
3. Also the alarm goes off, only when current time is equal to alarm time and the seconds digits of current time is equal to zero.
   For example, current time=03:12:00am    alarm time=03:12am, the alarm will goes off
                current time=03:12:01am    alarm time=03:12am, the alarm will not goes off
4. In "Set Alarm Time" mode, the current time is still going.

*/


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
int digitPosition[2];

QueueHandle_t inputQueue;
uint16_t lastkeytime = 0;

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

void init_digitSelectionBox_Clock()
{
    digitPosition[0] = 65;
    digitPosition[1] = 106;

    digitSelectionBox.x_pos = digitPosition[0];
    digitSelectionBox.y_pos = 55;
    digitSelectionBox.weight = 30;
    digitSelectionBox.height = 24;
    digitSelectionBox.status = 0;
}

void init_digitSelectionBox_Alarm()
{
    digitPosition[0] = 85;
    digitPosition[1] = 127;

    digitSelectionBox.x_pos = digitPosition[0];
    digitSelectionBox.y_pos = 55;
    digitSelectionBox.weight = 30;
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

void showCurrentTime()
{
    setFont(FONT_DEJAVU24);
    snprintf(buffer, 20, "%02d:%02d:%02d", myClock.hour, myClock.minute, myClock.second);
    print_xy(buffer, (display_width / 2) - 55, (display_height / 2) - 10);

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

void showAlarmTime()
{
    setFont(FONT_UBUNTU16);
    print_xy("Alarm Time:", 45, (display_height - 15));
    snprintf(buffer, 20, "%02d:%02d", myAlarmClock.hour, myAlarmClock.minute);
    print_xy(buffer, 147, (display_height - 15));

    setFont(FONT_SMALL);
    if(myAlarmClock.isAm == 1)
    {
        print_xy("AM", 195, 124);
    }
    else
    {
        print_xy("PM", 195, 124);
    }

    // Bottom separate line
    draw_rectangle(0, (display_height - 17), display_width, 1, rgbToColour(255, 255, 255));
}

void showMenu()
{
    draw_rectangle(menuSelectionBox.x_pos, menuSelectionBox.y_pos, menuSelectionBox.weight, menuSelectionBox.height, rgbToColour(100, 100, 100));

    setFont(FONT_SMALL);
    snprintf(buffer, 20, "Set Current Time");
    print_xy(buffer, 15, 1);
    snprintf(buffer, 20, "Set Alarm Time");
    print_xy(buffer, 135, 1);

    // Top separate line
    draw_rectangle(0, 14, display_width, 1, rgbToColour(255, 255, 255));
}

void incrementDigits_Clock()
{
    if(digitSelectionBox.status == 0)
    {
        myClock.hour++;
        if(myClock.hour == 12 && myClock.isAm == 1)
        {
            // 12:00 am => 12:00pm
            myClock.isAm = 0;
        }
        else if(myClock.hour == 12 && myClock.isAm == 0)
        {
            // 12:00pm => 00:00am
            myClock.isAm = 1;
            myClock.hour = 0;
        }
        else if(myClock.hour == 13)
        {
            // When 13:00 is reached, change to 01:00 (NO 13:00pm)
            myClock.hour = 1;
        }                   
    }
    else
    {
        if(myClock.minute == 59)
        {
            myClock.minute = 0;
        }
        else
        {
            myClock.minute++;
        }
    }
}

void incrementDigits_Alarm()
{
    if(digitSelectionBox.status == 0)
    {
        myAlarmClock.hour++;
        if(myAlarmClock.hour == 12 && myAlarmClock.isAm == 1)
        {
            // 12:00 am => 12:00pm
            myAlarmClock.isAm = 0;
        }
        else if(myAlarmClock.hour == 12 && myAlarmClock.isAm == 0)
        {
            // 12:00pm => 00:00am
            myAlarmClock.isAm = 1;
            myAlarmClock.hour = 0;
        }
        else if(myAlarmClock.hour == 13)
        {
            // When 13:00 is reached, change to 01:00 (NO 13:00pm)
            myAlarmClock.hour = 1;
        }                   
    }
    else
    {
        if(myAlarmClock.minute == 59)
        {
            myAlarmClock.minute = 0;
        }
        else
        {
            myAlarmClock.minute++;
        }
    }
}

void changeBacklightPos(int8_t *selectedOK)
{
    if(digitSelectionBox.status == 0)
    {
        // if backlight is on the hours digit, move to the minutes digit
        digitSelectionBox.status++;
    }
    else
    {
        if(*selectedOK == 0)
        {
            // if backlight is not on OK button, then move backlight to the OK button pos
            *selectedOK = 1;  // enable the backlight
        }
        else
        {
            // if backlight is on OK button, then move backlight to the hours digit by reset 'digitSelectedBox.status' to 0.
            *selectedOK = 0;  // disable the backlight
            digitSelectionBox.status = 0;
        }
    }
}

void setCurrentTimeMode()
{
    init_digitSelectionBox_Clock();

    /* Variable Declaration */
    // int8_t flashing = 1;
    int64_t current_time = 0;
    int64_t last_time = 0;

    int bottomButtonPressDuration = 20;
    int topButtonPressDuration = 20;
    int8_t selectedOK = 0;

    myClock.second = 0;

    while (1)
    {
        last_time = esp_timer_get_time() / (int64_t) 1000000;
        cls(rgbToColour(0, 0, 0));
        setFontColour(255, 255, 255);

        digitSelectionBox.x_pos = digitPosition[digitSelectionBox.status];  // loop through the hours digit and minute digit

        /* Anything need to draw on the screen should write here */
        /*** START ***/
        // If user hover the OK button, then backlight on the digit should hide and backlight of OK option should visiable
        if(selectedOK == 1) draw_rectangle(204, 114, 26, 17, rgbToColour(0, 172, 13));
        if(selectedOK == 0) draw_rectangle(digitSelectionBox.x_pos, digitSelectionBox.y_pos, digitSelectionBox.weight, digitSelectionBox.height, rgbToColour(98, 102, 109));
        showCurrentTime();

        setFont(FONT_UBUNTU16);
        print_xy("Set Current Time Mode", 1, 1);

        print_xy("OK", 205, 115);        
        /*** END!!! ***/

        send_frame();
        wait_frame();

        // bottom(left) button pressed event
        if(gpio_get_level(0) == 0)
        {
            if(bottomButtonPressDuration == 0)
            {
                if(selectedOK == 0)
                {
                    // When selected digit
                    incrementDigits_Clock();
                }
                else
                {
                    // When selected "OK" -> return -> go back to main screen
                    return;
                }
                bottomButtonPressDuration++;
            }
        }
        else
        {
            if(bottomButtonPressDuration != 0) bottomButtonPressDuration = 0;
        }

        // top(right) button pressed event
        if(gpio_get_level(35) == 0)
        {
            if(topButtonPressDuration == 0)
            {
                changeBacklightPos(&selectedOK);
                // flashing = 0;
                topButtonPressDuration++;
            }
        }
        else
        {
            if(topButtonPressDuration != 0) topButtonPressDuration = 0;
        }

        current_time = esp_timer_get_time() / (int64_t) 1000000;

        if(current_time != last_time)
        {
            // printf("current time: %lld\n", current_time);
            // if(flashing == 0) flashing = 1;
            // else flashing = 0;
        }
    }
}

void setAlarmTimeMode(int64_t *current_time, int64_t *last_time)
{
    init_digitSelectionBox_Alarm();

    // int8_t flashing = 1;

    int bottomButtonPressDuration = 20;
    int topButtonPressDuration = 20;

    int8_t selectedOK = 0;

    while (1)
    {
        *last_time = esp_timer_get_time() / (int64_t) 1000000;
        cls(rgbToColour(0, 0, 0));
        setFontColour(255, 255, 255);

        digitSelectionBox.x_pos = digitPosition[digitSelectionBox.status];  // loop through the hours digit and minute digit

        /* Anything need to draw on the screen should write here */
        /*** START ***/
        // If user hover the OK button, then backlight on the digit should hide and backlight of OK option should visiable and flashing
        if(selectedOK == 1) draw_rectangle(204, 114, 26, 17, rgbToColour(0, 172, 13));
        if(selectedOK == 0) draw_rectangle(digitSelectionBox.x_pos, digitSelectionBox.y_pos, digitSelectionBox.weight, digitSelectionBox.height, rgbToColour(98, 102, 109));

        setFont(FONT_DEJAVU24);
        snprintf(buffer, 20, "%02d:%02d", myAlarmClock.hour, myAlarmClock.minute);
        print_xy(buffer, (display_width / 2) - 35, (display_height / 2) - 10);
    
        setFont(FONT_UBUNTU16);
        if(myAlarmClock.isAm == 1)
        {
            print_xy("AM", 166, 58);
        }
        else
        {
            print_xy("PM", 166, 58);
        }

        setFont(FONT_UBUNTU16);
        print_xy("Set Alarm Time Mode", 1, 1);

        print_xy("OK", 205, 115);  

        /*** END!!! ***/

        send_frame();
        wait_frame();

        *current_time = esp_timer_get_time() / (int64_t) 1000000;

        // bottom(left) button pressed event
        if(gpio_get_level(0) == 0)
        {
            if(bottomButtonPressDuration == 0)
            {
                if(selectedOK == 0)
                {
                    // When selected digit
                    incrementDigits_Alarm();
                }
                else
                {
                    // When selected "OK" -> return -> go back to main screen
                    return;
                }

                bottomButtonPressDuration++;
            }

        }
        else
        {
            if(bottomButtonPressDuration != 0) bottomButtonPressDuration = 0;
        }
        

        // top(right) button pressed event
        if(gpio_get_level(35) == 0)
        {
            if(topButtonPressDuration == 0)
            {
                changeBacklightPos(&selectedOK);
                // flashing = 0;
                topButtonPressDuration++;
            }           
        }
        else
        {
            if(topButtonPressDuration != 0) topButtonPressDuration = 0;
        }
        

        if((*current_time) != (*last_time))
        {
            // printf("current time: %lld\n", current_time);
            incrementTime();
            clockLogic();
            // if(flashing == 0) flashing = 1;
            // else flashing = 0;
        }
    }
    
}

void main_function()
{
    initTime();
    init_menuSelectionBox();

    /* Variable Declaration */
    int64_t current_time = 0;
    int64_t last_time = 0;
    int64_t flashing_counter_start = 0;
    int64_t flashing_counter_stop = 0;
    int bottomButtonPressDuration = 0;
    int topButtonPressDuration = 0;
    int alarmGoesOff = 0;
    int backLightFlashing = 0;

    while (1)
    {
        last_time = esp_timer_get_time() / (int64_t) 1000000;
        flashing_counter_start = esp_timer_get_time() / (int64_t) 300000;
        cls(rgbToColour(0, 0, 0));
        setFontColour(255, 255, 255);

        /* Anything need to draw on the screen should write here 00:00:00*/
        /*** START ***/
        if(alarmGoesOff == 1)
        {
            if(backLightFlashing == 0)
            {
                draw_rectangle(0, 0, display_width, display_height, rgbToColour(255, 0, 0));
            }
            else
            {
                draw_rectangle(0, 0, display_width, display_height, rgbToColour(0, 255, 5));
            }
            
        }

        showCurrentTime();
        showAlarmTime();
        showMenu();
        /*** END!!! ***/

        send_frame();
        wait_frame();

        current_time = esp_timer_get_time() / (int64_t) 1000000;
        flashing_counter_stop = esp_timer_get_time() / (int64_t) 300000;

        // bottom(left) button pressed event
        if(gpio_get_level(0) == 0)
        {
            if(bottomButtonPressDuration == 0)
            {
                // this can be active when button is pressed, and only active once
                if(alarmGoesOff == 1) alarmGoesOff = 0;
                else menuStatusChange();
                bottomButtonPressDuration++;
            }
        }
        else
        {
            if(bottomButtonPressDuration != 0) bottomButtonPressDuration = 0;
        }

        // top(right) button pressed event
        if(gpio_get_level(35) == 0)
        {
            if(topButtonPressDuration == 0)
            {
                if(alarmGoesOff == 1)
                {
                    alarmGoesOff = 0;
                    topButtonPressDuration++;
                }
                else if(menuSelectionBox.status == 0)
                {
                    // set current time mode
                    setCurrentTimeMode();
                }
                else
                {
                    // set alarm time mode
                    setAlarmTimeMode(&current_time, &last_time);
                }
                bottomButtonPressDuration = 20;
                continue;
            }
        }
        else
        {
            if(topButtonPressDuration != 0) topButtonPressDuration = 0;
        }
        
        
        if(current_time != last_time)
        {
            // printf("current time: %lld\n", current_time);
            incrementTime();
            clockLogic();

            // If current time is equal alarm time
            if(myClock.hour == myAlarmClock.hour && myClock.minute == myAlarmClock.minute)
            {
                if(myClock.isAm == myAlarmClock.isAm)
                {
                    if(myClock.second == 0)
                    {
                        // printf("Alarm goes off!\n");
                        alarmGoesOff = 1;
                    }
                }
            }
        }
        if(flashing_counter_start != flashing_counter_stop && alarmGoesOff == 1)
        {
            // printf("%lld\n", temp2);
            if(backLightFlashing == 0) backLightFlashing = 1;
            else backLightFlashing = 0;
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

    main_function();
}
