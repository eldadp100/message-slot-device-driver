/*
    Message Reader. used to test the driver.
*/
#include "message_slot.h"
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

/*
    argv[1] = device path
    argv[2] = channel id
*/
int main(int argc, char **argv)
{
    int file_desc;
    int ret_val;
    int buff_size = 100;
    char msg[buff_size];

    file_desc = open(argv[1], O_RDWR);
    if (file_desc < 0)
    {
        perror("can't open device\n the error message:");
        return ERROR;
    }

    ret_val = ioctl(file_desc, IOCTL_SET_CAHNNEL_IDX, atoi(argv[2]));
    ret_val = read(file_desc, msg, buff_size);
    if (ret_val == 0)
    {
        if (buff_size == 0)
            return SUCCESS;
        else
        {
            perror("ERROR:");
            return ERROR;
        }
    }
    int rt = write(STDOUT_FILENO, msg, buff_size);
    if (rt != buff_size)
    {
        return ERROR;
    }
    close(file_desc);
    return SUCCESS;
}