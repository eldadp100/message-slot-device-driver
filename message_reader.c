/*
    Message Reader. used to test the driver.
*/
#include "message_slot.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
    int file_desc;
    int ret_val;
    file_desc = open("%s", argv[1], O_RDWR);
    if (file_desc < 0)
    {
        printf("Can't open");
        exit(-1);
    }

    char msg[100];
    ret_val = ioctl(file_desc, IOCTL_SET_CHANNEL, argv[2]);
    ret_val = read(file_desc, msg, 100);
    printf("First msg: %s \n", msg);


    close(file_desc);
    return 0;
}