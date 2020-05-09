/*
    Message Sender. used to test the driver.
*/
#include "message_slot.h"
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

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

    ret_val = ioctl(file_desc, IOCTL_SET_CHANNEL, argv[2]);
    ret_val = write(file_desc, "Hello", 6);
    printf("message sent");
    close(file_desc);
    return 0;
}