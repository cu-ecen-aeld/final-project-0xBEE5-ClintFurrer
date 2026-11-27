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

/**
 *  Setup QR reader.
 *  return 0 sucess -1 error
 */
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
 *  will read the sensor 2 times each time waiting 250us before the next read.
 *  returns 1 if data is ready to be read out.
 *  returns 0 no data is waiting.
 *  returns -1 error is read.
 */
int qr_code_rdy(tiny_code_reader_results_t *qr_sensor)
{
    char temp_len[2];

    for(int try = 0; try < 2; try++)
    {
        int stat = read(qr_sensor->qr_sens_fd, temp_len, 2);
        if (stat <0)
        {
            return -1;
        }
        qr_sensor->content_length = (temp_len[1] << 8) + temp_len[0];
        usleep(250000);
        if (qr_sensor->content_length != 0)
        {
            return 1;
        }
    }
    return 0;
}

/**
 *  Reads out data payload based on qr_code_rdy() function being called first. Does not check if data is ready!
 *  returns data payload length on success and -1 read error.
 * 
 */
int qr_code_read(tiny_code_reader_results_t *qr_sensor)
{
    int stat = read(qr_sensor->qr_sens_fd, qr_sensor->content_bytes, qr_sensor->content_length+2); //+2 this the lenght is still part of the read
    if (stat <0)
    {
        return -1;
    }
    return qr_sensor->content_length;
}

