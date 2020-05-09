/*
    Message Sender. used to test the driver.
*/
#include "message_slot.h"
#include <stdlib.h>
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

    ret_val = ioctl(file_desc, IOCTL_SET_CHANNEL, argv[2]);
    ret_val = write(file_desc, "Hello", 5);

    close(file_desc);
    return 0;
}