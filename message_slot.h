#define __KERNEL__
#define MODULE

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");

int device_open(struct inode *_inode, struct file *_file);
int device_ioctal(struct file *_file, unsigned int control_command, unsigned long command_parameter);
int device_read(struct file *_file, char *buff, size_t *buff_size, loff_t *file_offset);
int device_write(struct file *_file, char *buff, size_t *buff_size, loff_t *file_offset);
