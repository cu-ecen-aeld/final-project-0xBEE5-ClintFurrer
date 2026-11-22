#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h> 
#include <errno.h>
#include <linux/input.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <linux/rtc.h>

#include "user_interface.h"

void test(void);
void gps_show(void);
void time_show(void);
void temperature_show(void);
void prg_alarm_show(void);
void alarms_show(void);
int keyboard_handler(struct pollfd *pfds, struct input_event *ev);
int RTC_handler(struct pollfd *pfds, unsigned long *rtc_data);
int find_next_alarm(time_t now, time_t *new_alarm, alarm_t *alarm_lst);

static int test_bit(int bit, const unsigned long *arr)
{
    int arr_element;
    int bit_pos;
    int ret_val;
    
    arr_element = arr[bit / BITS_PER_LONG]; //find the array index
    bit_pos = bit % BITS_PER_LONG; //find bit index in array

    ret_val = (arr_element >> bit_pos) & 1; //shift to LSB position and mask

    return ret_val;
}

/* Returns 1 if the fd can report the keys a menu needs. */
static int is_menu_device(int fd)
{
    unsigned long bits[KEY_MAX / BITS_PER_LONG + 1];
    memset(bits, 0, sizeof(bits));

    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0)
        return 0;

    return test_bit(KEY_UP, bits) &&
           test_bit(KEY_DOWN, bits) &&
           test_bit(KEY_LEFT, bits) &&
           test_bit(KEY_ENTER, bits);
}

/* Fills fds[] with every matching device, returns the count. */
static int scan_devices(int *fds, int max)
{
    int count = 0;
    char path[64], name[256];

    for (int i = 0; i < MAX_EVENT && count < max; i++) 
    {
        snprintf(path, sizeof(path), "/dev/input/event%d", i); //make the string to open /dev/input/event...
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0)
        {
            continue;               /* gaps are fine, keep scanning */
        }
        if (is_menu_device(fd)) 
        {
            //found a good fd that supports needs
            name[0] = '\0';
            ioctl(fd, EVIOCGNAME(sizeof(name)), name); //query specific keys of input dev
            printf("Using %s (%s)\n", path, name);
            fds[count++] = fd;
        } 
        else 
        {
            close(fd); //device does not support needs...
        }
    }
    return count;
}

int main (void)
{
    int cmd_idx = 0;
    char lcd_buff[48];
    int lcd_len;

    cmd_member_t testA;
    testA.name = "hello";
    testA.handler = test;

    cmd_member_t gps_dis;
    gps_dis.name = "SHOW GPS INFO";
    gps_dis.handler = gps_show;

    cmd_member_t time_dis;
    time_dis.name = "SHOW CUR TIME";
    time_dis.handler = time_show;

    cmd_member_t temper_dis;
    temper_dis.name = "SHOW TEMP";
    temper_dis.handler = temperature_show;

    cmd_member_t prog_dis;
    prog_dis.name = "PROG ALARM";
    prog_dis.handler = prg_alarm_show;


    cmd_member_t supported_cmds[MAX_NUM_CMDS] = {testA, gps_dis, time_dis, temper_dis, prog_dis};

    static alarm_t alarms[MAX_ALARMS];   /* zero-initialized: all slots free */

    alarms[0].in_use_FLG = 1;
    alarms[0].kind = ALARM_ONCE;
    alarms[0].id = 1;
    alarms[0].at = 1795315620;

    alarms[1].in_use_FLG = 1;
    alarms[1].kind = ALARM_ONCE;
    alarms[1].id = 2;
    alarms[1].at = 1795316760;

    int fd = open("/dev/lcd", O_WRONLY);
    if (fd == -1)
    {
        printf("LCD not found\n");
        return -1;
    }

    int fd_rtc = open("/dev/rtc1", O_NONBLOCK | O_RDONLY);
    if (fd_rtc == -1)
    {
        close(fd);
        printf("RTC_1 was not found :(\n");
        return -1;
    }

    //scan for input devices
    int fds[MAX_DEVS];
    int num_dev = scan_devices(fds, MAX_DEVS);
    if (num_dev == 0)
    {
        return -1;
    }

    //setup the poll struct
    struct pollfd pfds[MAX_DEVS];
    //for (int i = 0; i < num_dev; i++) 
    //{
    if (num_dev < 2)
    {
        pfds[keyboard_dev].fd = fds[0];
        pfds[keyboard_dev].events = POLLIN;
    }
    //}

    pfds[rtc_dev].fd = fd_rtc;
    pfds[rtc_dev].events = POLLIN;

    num_dev = 2;

    /* Set the alarm 10 s from now (full date/time) */
    struct rtc_time now;
    ioctl(fd_rtc, RTC_RD_TIME, &now);
    struct rtc_wkalrm wk = { .enabled = 1, .time = now };
    wk.time.tm_sec += 10;                 /* normalize if you cross 60 */
    if (wk.time.tm_sec >= 60) { wk.time.tm_sec -= 60; wk.time.tm_min++; }
    ioctl(fd_rtc, RTC_WKALM_SET, &wk);
    ioctl(fd_rtc, RTC_AIE_ON, 0);

    for (;;)
    {
        if (poll(pfds, num_dev, -1) < 0) //sleep until event
        {
            if (errno == EINTR)
            {
                continue; //back to top IRQ happent
            }
            return -1;
        }


            struct input_event ev;
            unsigned long rtc_data;
            ssize_t ret_lcd;


            int rtc_stat = RTC_handler(pfds, &rtc_data);
            if (rtc_stat == 0)
            {
                printf("RTC alarm fired! test pass\n");
                struct rtc_time rt;
                ioctl(fd_rtc, RTC_RD_TIME, &rt); //read current time
                struct tm tmv = {0};
                tmv.tm_sec = rt.tm_sec;   tmv.tm_min = rt.tm_min;   tmv.tm_hour = rt.tm_hour;
                tmv.tm_mday = rt.tm_mday; tmv.tm_mon = rt.tm_mon;   tmv.tm_year = rt.tm_year;
                time_t now = timegm(&tmv);
                time_t next_alarm;
                int alarm_stat = find_next_alarm(now, &next_alarm, alarms);
                if (alarm_stat >= 0)
                {
                    struct tm tmv;
                    struct rtc_wkalrm wk;
                    gmtime_r(&next_alarm, &tmv);
                    memset(&wk, 0, sizeof(wk));
                    wk.enabled = 1;
                    wk.pending = 0;
                    wk.time.tm_sec = tmv.tm_sec;
                    wk.time.tm_min = tmv.tm_min;
                    wk.time.tm_hour = tmv.tm_hour;
                    wk.time.tm_mday = tmv.tm_mday;
                    wk.time.tm_mon  = tmv.tm_mon;
                    wk.time.tm_year = tmv.tm_year;

                    ioctl(fd_rtc, RTC_WKALM_SET, &wk); //TODO error check
                    ioctl(fd_rtc, RTC_AIE_ON, 0);
                }
                else
                {
                    ioctl(fd_rtc, RTC_AIE_OFF, 0);   /* nothing pending: disarm */
                }


                printf("found alarm idx is the earlist %d\n", alarm_stat);

            }

            while(1) //loop until all data is read out
            {
                int key_stat = keyboard_handler(pfds, &ev);
                printf("key_stat %d\n", key_stat);
                if (key_stat != 0)
                {
                    break;
                }

                switch(ev.code)
                {
                    case KEY_UP:
                        memset(lcd_buff, 0, sizeof(char));
                        printf("UP key pressed\n");
                        
                        printf("cmd_idx up:%d\n", cmd_idx);
                        lcd_len = snprintf(lcd_buff, sizeof(lcd_buff) - 1, "\033[Lx0y0;\033[Lk%s", supported_cmds[cmd_idx].name);
                        printf("%s\n", lcd_buff);
                        ret_lcd = write(fd, lcd_buff, lcd_len);                        
                        cmd_idx = (cmd_idx + 5 - 1) % 5;
                        break;
                    case KEY_DOWN:
                        memset(lcd_buff, 0, sizeof(char));
                        printf("key down pressed\n");
                        printf("cmd_idx dwn:%d\n", cmd_idx);
                        lcd_len = snprintf(lcd_buff, sizeof(lcd_buff) - 1, "\033[Lx0y0;\033[Lk%s", supported_cmds[cmd_idx].name);
                        printf("%s\n", lcd_buff);
                        ret_lcd = write(fd, lcd_buff, lcd_len);
                        cmd_idx = (cmd_idx + 5 + 1) % 5;;
                        if (ret_lcd == -1)
                        {

                        }
                        break;
                    case KEY_LEFT:
                        printf("key left pressed\n");
                        break;
                    case KEY_ENTER:
                        printf("enter pressed\n");
                        cmd_idx = (cmd_idx + 5 - 1) % 5;
                        supported_cmds[cmd_idx].handler();
                        break;
                }
            }
    }//for ever loop
}

int find_next_alarm(time_t now, time_t *new_alarm, alarm_t *alarm_lst)
{
    time_t best = 0;
    int best_idx = -1;
    for(int idx = 0; idx < MAX_ALARMS; idx++)
    {
        if(alarm_lst[idx].in_use_FLG == 1)
        {
            time_t cand = 0;
            switch (alarm_lst[idx].kind)
            {
                case ALARM_ONCE:
                    cand = alarm_lst[idx].at;
                break;
                case ALARM_DAILY:
                case ALARM_WEEKLY:

                break;
            }
            if (cand <= now)
            {
                continue; //the alarms is now or passed 
            }
            if ((best_idx == -1) || (cand < best))
            {
                best_idx = idx; //found an alarm in future
                best = cand;
            }
        }   
    }
    if (best_idx != -1)
    {
        *new_alarm = best; //value to set the RTC to
    }
    return best_idx; 
}

int keyboard_handler(struct pollfd *pfds, struct input_event *ev)
{
    ssize_t ret;

    if (pfds[keyboard_dev].revents & (POLLERR | POLLHUP | POLLNVAL)) 
    {
        //Unplugged: stop polling this fd (negative fd is ignored)
        close(pfds[keyboard_dev].fd);
        pfds[keyboard_dev].fd = -1; //mark as invalid
        return 2;
        //continue;
    }
    if (!(pfds[keyboard_dev].revents & POLLIN)) //check id fd has data to report
    {
        return 1; //no data skip device
    }

    while(1) //loop until all data read or error
    {
        ret = read(pfds[keyboard_dev].fd, ev, sizeof(*ev)); //read all the data out into ev
        if (ret == -1)
        {
            if (errno == EAGAIN)
            {
                return 1; //empty data
            }
            if (errno == ENODEV) //dev unplugged
            {
                close(pfds[keyboard_dev].fd);
                pfds[0].fd = -1;
                return 2;
            }
            fprintf(stderr, "kbd error: revents=0x%x fd=%d\n", pfds[keyboard_dev].revents, pfds[keyboard_dev].fd);
            fprintf(stderr, "kbd read error: %s\n", strerror(errno));
            return 1;
        }
        if (ret != (ssize_t)sizeof(*ev))
        {
            return 1; //did not get full event
        }

        if (ev->type != EV_KEY || ev->value == 0)
        {
            continue; //skip if release or non-key event
        }
        else
        {   //real key presses 
            return 0; //got all the key event data returning data
        }
    }
}

int RTC_handler(struct pollfd *pfds, unsigned long *rtc_data)
{
    ssize_t ret;

    if (pfds[rtc_dev].revents & (POLLERR | POLLHUP | POLLNVAL)) 
    {
        //Unplugged: stop polling this fd (negative fd is ignored)
        close(pfds[rtc_dev].fd);
        pfds[rtc_dev].fd = -1; //mark as invalid
        return 2;
        //continue;
    }
    if (!(pfds[rtc_dev].revents & POLLIN)) //check id fd has data to report
    {
        return 3; //no data skip device
    }

    ret = read(pfds[rtc_dev].fd, rtc_data, sizeof(*rtc_data)); //read all the data out into rtc_data
    if (ret == -1)
    {
        if (errno != EAGAIN)
        {
            fprintf(stderr, "rtc read error: %s\n", strerror(errno));
        }
        return 4;
    }
    if (ret != (ssize_t)sizeof(*rtc_data))
    {
        return 5; //did not get full event
    }
    return (*rtc_data & RTC_AF) ? 0 : 1; //RTC_AF (0x20) Alarm_flag masked with data
}

void test(void)
{
    printf("test menu\n");
}

void gps_show(void)
{
    printf("gps menu\n");
}

void time_show(void)
{
    printf("time menu\n");
}

void temperature_show(void)
{
    printf("temperature menu\n");
}

void prg_alarm_show(void)
{
    printf("read QR code\n");
}

void alarms_show(void)
{
    printf("show alarms\n");
}