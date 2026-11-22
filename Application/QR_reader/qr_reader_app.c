#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <string.h>

#include "qr_reader_app.h"

#define I2C_DEV_PATH "/dev/i2c-1"
#define SLAVE_ADDR   0x0C 

int main(void)
{
    int fd;
    //tiny_code_reader_results_t code_data;
    char data[256];

    printf("Hello Test QR Read\n");

    memset(data, 0, sizeof(char));

    fd = open(I2C_DEV_PATH, O_RDWR);
    if (fd < 0)
    {
        printf("failed to open i2c device\n");
        return -1;
    }

    //set slave address
    if (ioctl(fd, I2C_SLAVE, SLAVE_ADDR) < 0)
    {
        printf("ioctl failed\n");
        close(fd);
        return -1;
    }

    int loopFLG = 0;
    while(loopFLG == 0)
    {
        int stat = read(fd, data, 2);
        if (stat <0)
        {

        }
        //printf("read stat is %d\n", stat);
        //printf("Sensor read length %02x %02x\n", code_data.content_bytes[0], code_data.content_bytes[1]);
        if (data[0] != 0)
        {
            loopFLG = 1;
        }
        usleep(250000);
    }

    int length = data[0];
    printf("length is..%d\n", length);
    int stat = read(fd, data, length+2);
    if (stat <0)
    {

    }
    //code_data.content_bytes++;
    //code_data.content_bytes++;
    printf("read stat is %d\n", stat);
    //printf("code reads as %s\n", data);
    printf("Sensor read length %02x %02x\n", data[0], data[1]);
    for(int idx = 0; idx < 100; idx++)
    {
        printf("%c", data[idx]);
    }

    stat = read(fd, data, 2);
    printf("\nclose out read stat is %d\n", stat);
    printf("Sensor read length %02x %02x\n", data[0], data[1]);
    usleep(200000);
    stat = read(fd, data, 2);
    printf("\nclose out read stat is %d\n", stat);
    printf("Sensor read length %02x %02x\n", data[0], data[1]);

    printf("\napp is out\n");
    close(fd);
    return 0;
}