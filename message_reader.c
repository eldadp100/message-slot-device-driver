/*
    Message Reader. used to test the driver.
*/
#include "message_slot.h"
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

#define BUFF_SIZE 128

/*
    argv[1] = device path
    argv[2] = channel id
*/

int main(int argc, char **argv)
{
    int file_desc;
    int ret_val;
    char msg[BUFF_SIZE];
    memset(msg, 0, BUFF_SIZE);

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
    ret_val = read(file_desc, msg, BUFF_SIZE);
    if (ret_val == -1)
    {
        perror("READ FROM DEVICE FAILED: ");
        return 1;
    }
    ret_val = write(STDOUT_FILENO, msg, ret_val);
    if (ret_val == 0)
    {
        perror("WRITE TO STDOUT FAILED: ");
        return 1;
    }

    close(file_desc);
    return 0;
}