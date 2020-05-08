#include <linux/ioctl.h>

#define MAJOR_NUM 240

// Set the message of the device driver
#define IOCTL_SET_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)
#define DEVICE_RANGE_NAME "message_slot"
#define DEVICE_FILE_NAME "message_slot"
