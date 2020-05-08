#include <linux/ioctl.h>

#define MAJOR_NUM 240

// Set the message of the device driver
#define IOCTL_SET_ENC _IOW(MAJOR_NUM, 0, unsigned long)

