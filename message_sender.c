/*
    Message Sender. used to test the driver.
*/
#include "message_slot.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>


/*
    argv[1] = device path
    argv[2] = channel id
    argv[3] = message to pass through channel
*/
int main(int argc, char **argv)
{
    int file_desc;
    int ret_val;


    file_desc = open(argv[1], O_RDWR);
    if (file_desc < 0)
    {
        perror("can't open device\n the error message:");
        return ERROR;
    }

    ret_val = ioctl(file_desc, IOCTL_SET_CAHNNEL_IDX, atoi(argv[2]));
    ret_val = write(file_desc, argv[3], strlen(argv[3]) + 1);
    printf("message sent succefully\n");
    close(file_desc);
    return SUCCESS;
}