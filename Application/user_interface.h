

#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

#define MAX_NUM_CMDS 8

typedef void (*cmd_handler_t) (void);

typedef struct
{
    const char *name;
    cmd_handler_t handler;
} cmd_member_t;



#endif /* USER_INTERFACE_H */