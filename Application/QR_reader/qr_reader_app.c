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


int qr_code_init(tiny_code_reader_results_t *qr_sensor)
{
    int fd;
    printf("Hello Test QR Read\n");
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
    qr_sensor->qr_sens_fd = fd;
    return 0;    
}

/**
 * 
 * 
 */
int qr_code_rdy(tiny_code_reader_results_t *qr_sensor)
{
    int stat = read(qr_sensor->qr_sens_fd, qr_sensor->content_bytes, 2);
    if (stat <0)
    {

    }
    usleep(250000);
    if (qr_sensor->content_bytes[0] != 0)
    {
        return 1;
    }
    return 0;
}

/**
 * 
 * 
 */
int qr_code_read(tiny_code_reader_results_t *qr_sensor)
{
    int stat;
    stat = read(qr_sensor->qr_sens_fd, qr_sensor->content_bytes, 2);
    qr_sensor->content_length = qr_sensor->content_bytes[0];
    //printf("length is..%d\n", length);
    stat = read(qr_sensor->qr_sens_fd, qr_sensor->content_bytes, qr_sensor->content_length+2);
    if (stat <0)
    {

    }
    return qr_sensor->content_length;
}

