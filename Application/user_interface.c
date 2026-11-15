#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h> 
#include <errno.h>
#include <linux/input.h>
#include <poll.h>

#include "user_interface.h"


//cmd_member_t supported_cmds[8];

void test(void);
void gps_show(void);
void time_show(void);
void temperature_show(void);
void prg_alarm_show(void);
void alarms_show(void);

#define MAX_DEVS   8
#define MAX_EVENT  32
#define BITS_PER_LONG (sizeof(unsigned long) * 8)

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

    cmd_member_t testA;
    testA.name = "hello";
    testA.handler = test;

    cmd_member_t gps_dis;
    gps_dis.name = "SHOW GPS INFO";
    gps_dis.handler = gps_show;

    cmd_member_t time_dis;
    time_dis.name = "SHOW CURRENT TIME";
    time_dis.handler = time_show;

    cmd_member_t temper_dis;
    temper_dis.name = "SHOW TEMPERATURE";
    temper_dis.handler = temperature_show;

    cmd_member_t prog_dis;
    prog_dis.name = "PROGRAM ALARM";
    prog_dis.handler = prg_alarm_show;


    cmd_member_t supported_cmds[MAX_NUM_CMDS] = {testA, gps_dis, time_dis, temper_dis, prog_dis};

    int fd = open("/dev/lcd", O_WRONLY);
    if (fd == -1)
    {

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
    for (int i = 0; i < num_dev; i++) 
    {
        pfds[i].fd = fds[i];
        pfds[i].events = POLLIN;
    }

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
        //loop through device/s
        for(int idx = 0; idx < num_dev; idx++)
        {
            if (pfds[idx].revents & (POLLERR | POLLHUP | POLLNVAL)) 
            {
            //Unplugged: stop polling this fd (negative fd is ignored)
            close(pfds[idx].fd);
            pfds[INPUT_KEYMAP_BY_INDEX].fd = -1; //mark as invalid
            continue;
            }
            if (!(pfds[idx].revents & POLLIN)) //check id fd has data to report
            {
                continue; //no data skip device
            }

            struct input_event ev;
            ssize_t ret;
            ssize_t ret_lcd;
            
            char lcd_buff[48];
            int lcd_len;
            while(1) //loop until all data is read out
            {
                ret = read(pfds[idx].fd, &ev, sizeof(ev));
                if (ret != sizeof(ev))
                {
                    break; //did not get full event
                }
                if (ev.type != EV_KEY || ev.value == 0)
                {
                    continue; //skip if release or non-key event
                }

                switch(ev.code)
                {
                    case KEY_UP:
                        printf("UP key pressed\n");
                        
                        printf("cmd_idx up:%d\n", cmd_idx);
                        lcd_len = snprintf(lcd_buff, sizeof(lcd_buff) - 1, "\r (\x0d)%s", supported_cmds[cmd_idx].name);
                        printf("%s\n", lcd_buff);
                        ret_lcd = write(fd, lcd_buff, lcd_len);                        
                        cmd_idx = (cmd_idx + 5 - 1) % 5;
                        break;
                    case KEY_DOWN:
                        printf("key down pressed\n");
                        printf("cmd_idx dwn:%d\n", cmd_idx);
                        lcd_len = snprintf(lcd_buff, sizeof(lcd_buff) - 1, "\r (\x0d)%s", supported_cmds[cmd_idx].name);
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
                        supported_cmds[cmd_idx].handler();
                        break;
                }
            }
            if (ret < 0 && errno == ENODEV)
            {
                close(pfds[idx].fd);
                pfds[idx].fd = -1;
            }
        } //for loop
    }//for ever loop
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