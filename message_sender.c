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
        perror("CAN'T OPEN DEVICE: ");
        return 1;
    }

    ret_val = ioctl(file_desc, MSG_SLOT_CHANNEL, atoi(argv[2]));
    if (ret_val == -1)
    {
        perror("IOCTL TO DEVICE FAILED: ");
        return 1;
    } 
    ret_val = write(file_desc, argv[3], strlen(argv[3]));
    if (ret_val == -1)
    {
        perror("WRITE TO DEVICE FAILED: ");
        return 1;
    }

    close(file_desc);
    return 0;
}