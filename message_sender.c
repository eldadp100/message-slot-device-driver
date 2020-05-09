/*
    Message Sender. used to test the driver.
*/
#include "message_slot.h"
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

int main(int argc, char **argv)
{
    int file_desc;
    int ret_val;


    file_desc = open(argv[1], O_RDWR);
    if (file_desc < 0)
    {
        printf("Can't open");
        exit(-1);
    }

    ret_val = ioctl(file_desc, IOCTL_SET_CHANNEL, argv[3]);
    ret_val = write(file_desc, argv[2], strlen(argv[2]) + 1);
    printf("message sent");
    close(file_desc);
    return 0;
}