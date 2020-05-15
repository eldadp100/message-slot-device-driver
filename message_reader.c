/*
    Message Reader. used to test the driver.
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
        printf("Can't open\n");
        exit(-1);
    }

    char msg[100];
    ret_val = ioctl(file_desc, IOCTL_SET_CAHNNEL_IDX, atoi(argv[2]));
    ret_val = read(file_desc, msg, 100);
    printf("msg: %s \n", msg);


    close(file_desc);
    return 0;
}