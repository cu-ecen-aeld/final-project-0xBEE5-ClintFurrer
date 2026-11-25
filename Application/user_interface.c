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
#include <sys/timerfd.h>
#include <cjson/cJSON.h>
//#include <pthread.h>

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
int save_alarm_list(alarm_t *alarm_lst);
int json_alarms_to_alarm_lst(alarm_t *alarm_lst);

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

    json_alarms_to_alarm_lst(alarms);

/*
    alarms[0].in_use_FLG = 1;
    alarms[0].kind = ALARM_ONCE;
    alarms[0].id = 1;
    alarms[0].at = 1795312063 + 200;
    snprintf(alarms[0].name, sizeof(alarms[0].name), "%s", "test1");

    alarms[1].in_use_FLG = 1;
    alarms[1].kind = ALARM_ONCE;
    alarms[1].id = 2;
    alarms[1].at = 1795312063 + 150;
    snprintf(alarms[1].name, sizeof(alarms[1].name), "%s", "test2");

    alarms[2].in_use_FLG = 1;
    alarms[2].kind = ALARM_WEEKLY;
    alarms[2].id = 3;
    alarms[2].day_mask = 0x07;
    alarms[2].time_of_day = 64800;
    snprintf(alarms[3].name, sizeof(alarms[3].name), "%s", "test_barf");
*/
    printf("debug alarm list\n");
    printf("alarm id %d\n", alarms[0].id);
    printf("alarm name %s\n", alarms[0].name);
    printf("alarm at %ld\n", alarms[0].at);
    printf("alarm weekly mask %d\n", alarms[2].day_mask);
    printf("time of day %d\n", alarms[2].time_of_day);

    int lcd_fd = open("/dev/lcd", O_WRONLY);
    if (lcd_fd == -1)
    {
        printf("LCD not found\n");
        return -1;
    }

    int rtc_fd = open("/dev/rtc1", O_NONBLOCK | O_RDONLY);
    if (rtc_fd == -1)
    {
        close(lcd_fd);
        printf("RTC_1 was not found :(\n");
        return -1;
    }

    //scan for input devices
    int fds[MAX_DEVS];
    int num_dev = scan_devices(fds, MAX_DEVS);
    if (num_dev == 0)
    {
        close(lcd_fd);
        close(rtc_fd);
        return -1;
    }

    int qr_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (qr_fd == -1)
    {
        close(lcd_fd);
        close(rtc_fd);
        //TODO close fds
        return -1;
    }

    int timeD_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timeD_fd == -1)
    {
        close(lcd_fd);
        close(rtc_fd);
        //TODO close fds
        close(qr_fd);
        return -1;
    }

    struct itimerspec ts_qr = {
    .it_value = { .tv_sec = 0, .tv_nsec = 250000000 },    // Initial expiration: 
    .it_interval = { .tv_sec = 0, .tv_nsec = 250000000 }  // Repeat interval: 
    };

    struct itimerspec ts_timeD = {
    .it_value = { .tv_sec = 0, .tv_nsec = 800000000 },    // Initial expiration: 
    .it_interval = { .tv_sec = 0, .tv_nsec = 800000000 }  // Repeat interval: 
    };

    if (timerfd_settime(qr_fd, 0, &ts_qr, NULL) == -1) 
    {
        printf("QR timerfd_settime error\n");
        close(qr_fd);
        return -1;
    }
    
    if (timerfd_settime(timeD_fd, 0, &ts_timeD, NULL) == -1) 
    {
        printf("QR timerfd_settime error\n");
        close(timeD_fd);
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

    pfds[rtc_dev].fd = rtc_fd;
    pfds[rtc_dev].events = POLLIN;

    pfds[qr_timer].fd = qr_fd;
    pfds[qr_timer].events = POLLIN;

    pfds[timeD_timer].fd = timeD_fd;
    pfds[timeD_timer].events = POLLIN;


    
/*
    int save_stat = save_alarm_list(alarms);
    if (save_stat < 0)
    {
        printf("save alarm failed %d\n", save_stat);
    }


    
    struct rtc_time now;
    ioctl(fd_rtc, RTC_RD_TIME, &now);
    struct rtc_wkalrm wk = { .enabled = 1, .time = now };
    wk.time.tm_sec += 10;                 // normalize if you cross 60 
    if (wk.time.tm_sec >= 60) { wk.time.tm_sec -= 60; wk.time.tm_min++; }
    ioctl(fd_rtc, RTC_WKALM_SET, &wk);
    ioctl(fd_rtc, RTC_AIE_ON, 0);
*/


    struct tm tmv = {0};
    time_t now = timegm(&tmv);
    time_t next_alarm;
    int alarm_stat = find_next_alarm(now, &next_alarm, alarms);
    //int alarm_stat = -1;
    for (;;)
    {
        if (poll(pfds, num_devices, -1) < 0) //sleep until event
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
                printf("RTC alarm fired!\n");
                printf("alarm stat %d\n", alarm_stat);
                if (alarm_stat >= 0)
                {
                    printf("%s\n", alarms[alarm_stat].name);
                }
                struct rtc_time rt;
                ioctl(rtc_fd, RTC_RD_TIME, &rt); //read current time
                struct tm tmv = {0};
                tmv.tm_sec = rt.tm_sec;   tmv.tm_min = rt.tm_min;   tmv.tm_hour = rt.tm_hour;
                tmv.tm_mday = rt.tm_mday; tmv.tm_mon = rt.tm_mon;   tmv.tm_year = rt.tm_year;
                time_t now = timegm(&tmv);
                time_t next_alarm;
                alarm_stat = find_next_alarm(now, &next_alarm, alarms);
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

                    printf("alarm: %04d-%02d-%02d %02d:%02d:%02d UTC (enabled=%d)\n",
                        wk.time.tm_year + 1900,
                        wk.time.tm_mon + 1,
                        wk.time.tm_mday,
                        wk.time.tm_hour,
                        wk.time.tm_min,
                        wk.time.tm_sec,
                        wk.enabled);

                    char buf[32];
                    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
                    printf("alarm: %s UTC\n", buf);  
                    ioctl(rtc_fd, RTC_WKALM_SET, &wk); //TODO error check
                    ioctl(rtc_fd, RTC_AIE_ON, 0);
                    printf("new alarm found and armed! alarm idx %d\n", alarm_stat);
                }
                else
                {
                    ioctl(rtc_fd, RTC_AIE_OFF, 0);   /* nothing pending: disarm */
                    printf("no alarms are pending...\n");
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
                        ret_lcd = write(lcd_fd, lcd_buff, lcd_len);                        
                        cmd_idx = (cmd_idx + 5 - 1) % 5;
                        break;
                    case KEY_DOWN:
                        memset(lcd_buff, 0, sizeof(char));
                        printf("key down pressed\n");
                        printf("cmd_idx dwn:%d\n", cmd_idx);
                        lcd_len = snprintf(lcd_buff, sizeof(lcd_buff) - 1, "\033[Lx0y0;\033[Lk%s", supported_cmds[cmd_idx].name);
                        printf("%s\n", lcd_buff);
                        ret_lcd = write(lcd_fd, lcd_buff, lcd_len);
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

static void alarm_type_lookup(alarm_kind_t kind, char *data)
{
    switch(kind)
    {
        case ALARM_ONCE:
        strcpy(data, "once");
        break;

        case ALARM_DAILY:
        strcpy(data, "daily");
        break;
        
        case ALARM_WEEKLY:
        strcpy(data, "weekly");
        break;

        default:
        strcpy(data, "error");
        break;
    }
}

static alarm_kind_t type_alarm_type_lookup(char *data)
{
    const char *types[] = {"once", "daily", "weekly"};
    alarm_kind_t temp = ERROR;

    for(int idx = 0; idx < 3; idx++)
    {
        if (strcmp(data, types[idx]) == 0)
        {
            temp = idx;
            break;
        }
    }
    return temp;
}

/**
 *  Save off the alarm array list to a JSON file 
 *  for power cycle and reboot alarm list retention.
 */
int save_alarm_list(alarm_t *alarm_lst)
{
    ssize_t ret = 1;
    cJSON *root = cJSON_CreateObject(); //make base json object
    if (root == NULL)
    {
        printf("root failed in json \n ");
        cJSON_Delete(root);
        return -1;
    }
    
    cJSON *myarr = cJSON_CreateArray(); //make an array for alarms
    if (myarr == NULL)
    {
        printf("array create failed in json\n");
        cJSON_Delete(root);
        cJSON_Delete(myarr);
        return -1;
    }

    cJSON_AddNumberToObject(root, "version", 1); //add version number
    cJSON_AddItemToObject(root, "alarms", myarr);   /* root owns arr now */
    //walk through the alarm array and add active alarms into json array as objects
    char kind_data[10];
    for(int idx = 0; idx < MAX_ALARMS; idx++)
    {
        if (alarm_lst[idx].in_use_FLG == 1)
        {
            cJSON *obj = cJSON_CreateObject(); //make object
            if (!obj)
            {
                printf("object create failed in json\n");
                cJSON_Delete(root);
                return -1;
            }
            cJSON_AddNumberToObject(obj, "id", alarm_lst[idx].id);
            cJSON_AddStringToObject(obj, "name", alarm_lst[idx].name);
            alarm_type_lookup(alarm_lst[idx].kind, kind_data);
            cJSON_AddStringToObject(obj, "repeat", kind_data);
            if (alarm_lst[idx].kind == ALARM_ONCE)
            {
                cJSON_AddNumberToObject(obj, "at", alarm_lst[idx].at);
            }
            else
            {
                cJSON_AddNumberToObject(obj, "time", alarm_lst[idx].time_of_day);
                cJSON *days = cJSON_AddArrayToObject(obj, "days");
                for (int w = 0; w < 7; w++)
                {
                    if (alarm_lst[idx].day_mask & (1u << w))
                    {
                        cJSON_AddItemToArray(days, cJSON_CreateNumber(w));
                    }
                }
            }
            cJSON_AddItemToArray(myarr, obj); //add object to the array
        }
    }

    char *json_str = cJSON_Print(root); //make json object into string to write to file
    const char *json_str_ptr = json_str;
    cJSON_Delete(root);
    size_t datalen = strlen(json_str);
    int fd = open(ALARM_FILE ".tmp", O_WRONLY | O_CREAT | O_TRUNC, 0664); //make tmp file in case it fails OG file is safe
    if (fd < 0)
    {
        printf("file create failed\n");
        cJSON_free(json_str);
        cJSON_Delete(root);
        return -1;
    }
    
    while ((datalen != 0) && (ret != 0))
    {
        printf("str_ptr=%p datalen=%zu\n", (const void *)json_str_ptr, datalen);
        ret = write(fd, json_str_ptr, datalen);
        if (ret == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            printf("write error %d\n", errno);
            break;
        }
        datalen -= ret;
        json_str_ptr += ret;
    }
    //clean up json object and push file and close
    printf("finishing save\n");
    fsync(fd);
    cJSON_free(json_str);
   
    close(fd);
    rename(ALARM_FILE ".tmp", ALARM_FILE); //make now OG file
    printf("alarms saved\n");
    return ret;
}

/**
 * 
 */
int json_alarms_to_alarm_lst(alarm_t *alarm_lst)
{
    int idx = 0;

    memset(alarm_lst, 0, sizeof(alarm_t) * MAX_ALARMS);

    int fd = open(ALARM_FILE, O_RDONLY ); 
    if (fd < 0)
    {
        printf("file open failed\n");
        return -1;
    }

    char buffer[2048];
    char *buf_ptr = buffer;
    ssize_t ret = 1;
    size_t datalen = sizeof(buffer);
    while ((datalen != 0) && (ret != 0))
    {
        ret = read(fd, buf_ptr, datalen);
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            printf("read error %d\n", errno);
            close(fd);
            return -1;
        }
        datalen -= ret;
        buf_ptr += ret;
        if (datalen >= sizeof(buffer) - 1)
        {
            break; //file to big
        }
    }
    buf_ptr++;
    *buf_ptr = '\0';
    close(fd);

    // parse the JSON data
    cJSON *json = cJSON_Parse(buffer);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            printf("Error: %s\n", error_ptr);
        }
        cJSON_Delete(json);
        return -1;
    }

    cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
    if (cJSON_IsNumber(version) && (version->valueint == 1))
    {
        cJSON *arr = cJSON_GetObjectItemCaseSensitive(json, "alarms");
        if (cJSON_IsArray(arr) == 1)
        {
            cJSON *alarm_member = NULL;
            cJSON_ArrayForEach(alarm_member, arr) //for loop
            {
                cJSON *id = cJSON_GetObjectItemCaseSensitive(alarm_member, "id");
                cJSON *name = cJSON_GetObjectItemCaseSensitive(alarm_member, "name");
                cJSON *repeat = cJSON_GetObjectItemCaseSensitive(alarm_member, "repeat");

                if (cJSON_IsString(repeat) && (repeat->valuestring != NULL))
                {
                    alarm_lst[idx].kind = type_alarm_type_lookup(repeat->valuestring);
                }
                else
                {
                    continue;
                }

                if (cJSON_IsNumber(id) == 1)
                {
                    alarm_lst[idx].id = id->valueint;
                }
                else
                {
                    continue;
                }

                if (cJSON_IsString(name) && (name->valuestring != NULL))
                {
                    snprintf(alarm_lst[idx].name, sizeof(alarm_lst[idx].name), "%s", name->valuestring);
                }

                switch(alarm_lst[idx].kind)
                {
                    case ALARM_ONCE: {
                        cJSON *at = cJSON_GetObjectItemCaseSensitive(alarm_member, "at");
                        if (cJSON_IsNumber(at) == 1)
                        {
                            alarm_lst[idx].at = (time_t)at->valuedouble;
                        }
                        break;
                    }
                    case ALARM_DAILY: {
                        alarm_lst[idx].day_mask = 0x7F; //all days set
                        cJSON *tod = cJSON_GetObjectItemCaseSensitive(alarm_member, "time");
                        if (cJSON_IsNumber(tod) == 1)
                        {
                            if (tod->valueint < 0 || tod->valueint > 86399)
                            {
                                continue; //out of range skipping
                            }
                            alarm_lst[idx].time_of_day = tod->valueint;
                        }
                        break;
                    }
                    case ALARM_WEEKLY: {
                        cJSON *tod_wk = cJSON_GetObjectItemCaseSensitive(alarm_member, "time");
                        if (cJSON_IsNumber(tod_wk) == 1)
                        {
                            if (tod_wk->valueint < 0 || tod_wk->valueint > 86399)
                            {
                                continue; //out of range skipping
                            }
                            alarm_lst[idx].time_of_day = tod_wk->valueint;
                        }
                        cJSON *days = cJSON_GetObjectItemCaseSensitive(alarm_member, "days");
                        if (!cJSON_IsArray(days))
                        {
                            continue;
                        }
                        uint8_t mask = 0;
                        cJSON *d;
                        cJSON_ArrayForEach(d, days)
                        {
                            if (!cJSON_IsNumber(d))
                            {
                                continue;
                            }
                            int wday = d->valueint;
                            if (wday < 0 || wday > 6)
                            {
                                continue;
                            }
                            mask |= (uint8_t)(1u << wday);  /* set that weekday's bit */
                        }

                        if (mask == 0)
                        {
                            continue;                      /* empty list: alarm could never fire */
                        }
                        alarm_lst[idx].day_mask = mask;
                        break;
                    }
                    default: {
                     break;
                    }
                }
                alarm_lst[idx].in_use_FLG = 1;
                idx++;
                if(idx >= MAX_ALARMS)
                {
                    break;
                }
            }
        }
    }    
    cJSON_Delete(json);
    return 0;
}

/**
 * The + 4 is because 1 Jan 1970 was a Thursday (wday 4).
 * Looping d from 0 to 7 covers today (if the time hasn’t passed) 
 * through the same weekday next week, so a weekly alarm whose only day is 
 * today but already passed still finds next week.
 * If day_mask is 0, nothing matches and it returns 0, so a bad entry is 
 * ignored instead of looping forever.
 */
static time_t next_recurring(const alarm_t *a, time_t now)
{
    time_t day0 = now - (now % 86400);          /* today's UTC midnight */
    for (int d = 0; d <= 7; d++)
    {
        time_t cand = day0 + (time_t)d * 86400 + a->time_of_day;
        int wday = (int)(((day0 / 86400) + d + 4) % 7);   /* 0 = Sunday */
        if (cand > now && (a->day_mask & (1 << wday)))
            return cand;
    }
    return 0;
}

/**
 * 
 */
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
                    cand = next_recurring(&alarm_lst[idx], now);
                break;
                default:
                break;
            }
            if (cand <= now)
            {
                alarm_lst[idx].in_use_FLG = 0; //take out service
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

/**
 * 
 */
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

int QR_code_handler(struct pollfd *pfds)
{
    ssize_t ret;

    if (pfds[qr_timer].revents & (POLLERR | POLLHUP | POLLNVAL)) 
    {
        //Unplugged: stop polling this fd (negative fd is ignored)
        close(pfds[qr_timer].fd);
        pfds[qr_timer].fd = -1; //mark as invalid
        return 2;
    }
    if (!(pfds[qr_timer].revents & POLLIN)) //check id fd has data to report
    {
        return 3; //no data skip device
    }

    uint64_t timerData;
    ret = read(pfds[qr_timer].fd, timerData, sizeof(timerData)); //read all the data out into rtc_data
    if (ret == -1)
    {
        if (errno != EAGAIN)
        {
            fprintf(stderr, "QR timer read error: %s\n", strerror(errno)); //TODO don't like this
        }
        return 4;
    }   

    return 0;
}


/**
 * 
 */
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
            fprintf(stderr, "rtc read error: %s\n", strerror(errno)); //TODO don't like this
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