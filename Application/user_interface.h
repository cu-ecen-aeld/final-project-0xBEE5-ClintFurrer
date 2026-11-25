

#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define ALARM_FILE "/etc/alarms.json"
#define MAX_ALARMS   16
#define DAYS_ALL     0x7F
#define MAX_NUM_CMDS 8

#define MAX_DEVS   8
#define MAX_EVENT  32
#define BITS_PER_LONG (sizeof(unsigned long) * 8)

enum
{
    keyboard_dev,
    rtc_dev,
    qr_timer,
    timeD_timer,
    num_devices
};

typedef void (*cmd_handler_t) (void);

typedef struct
{
    const char *name;
    cmd_handler_t handler;
} cmd_member_t;

typedef enum {
    ALARM_ONCE,  //uses at only
    ALARM_DAILY, //uses time_of_day with day_mask 0x7F
    ALARM_WEEKLY, //uses time_of_day and day_mask
    ERROR
} alarm_kind_t;

typedef struct
{
    bool in_use_FLG;
    int id;
    alarm_kind_t kind;
    int time_of_day; //seconds since midnight
    uint8_t day_mask;
    time_t at;
    char name[17];
} alarm_t;



#endif /* USER_INTERFACE_H */