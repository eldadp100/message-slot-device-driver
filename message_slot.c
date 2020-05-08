/*
    This file implemets the message slot IPC driver.
    To add (register) the module to the kernel use:
    To remove the module: 
*/

// TODO: don't limit number of channels and minor numbers

#define __KERNEL__
#define MODULE

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");

#define DEVICE_RANGE_NAME "message_slot"
#define DEVICE_FILE_NAME "message_slot"
#define MAJOR_NUM 240
#define MAX_MINORS 10
#define CHANNELS_NUM 8

typedef struct dict_entry
{
    int key;
    void *val;
} dict_entry_t;

typedef struct simple_dict
{
    int max_etries;
    int current_entry_index;
    dict_entry_t *entries;
} simple_dict_t;

simple_dict_t *create_simple_dict(int max_number_of_keys, int sizeof_value)
{
    simple_dict_t *dict = malloc(sizeof(simple_dict_t));
    dict->entries = malloc(max_number_of_keys * sizeof(int) + sizeof_value);
    dict->current_entry_index = 0;
    dict->max_etries = max_number_of_keys;
    return dict;
}

void *get_value(simple_dict_t *dict, int key)
{
    for (int i = 0; i < dict->dict_size; i++)
    {
        if (key == dict->keys[i])
            return &dict->values[i];
    }
    return NULL;
}

void set_value(simple_dict_t *dict, int key, int value)
{
    if (dict->current_entry_index == dict->max_etries - 1)
    {
        printk(KERN_ERR "simple dict is out of bounds.\n");
        exit(1);
    }
    dict->values[key] = value;
    dict->dict_size++;
}

typedef struct channels
{
    size_t current_channel;
    char **message_channels;
} channels_t;

channels_t create_new_channel()
{
    channels_t *new_channels = malloc(sizeof(channels_t));
    new_channels->message_channels = malloc(CHANNELS_NUM * sizeof(char *));
    new_channels->current_channel = 0;
    return new_channels;
}

simple_dict_t *minors_to_channels = NULL;

int initialize_minor_to_channels(int number_of_minors)
{
    minors_to_channels = create_simple_dict();
    return 0;
}

channels_t *get_channels_obj_of_minor(int minor_num, channels_t *out)
{
    if (minors_to_channels == NULL)
    {
        printk(KERN_ERR "minors_to_channels object is not initialized\n");
        exit(1);
    }
    return (*channels_t)get_value(minors_to_channels, minor_num);
}

int add_minor(int minor_num)
{
    channels_t *new_channel = create_new_channel();
    set_value(minors_to_channels, minor_num, new_channel);
    return 0;
}

static spinlock_t device_lock;
static int dev_open_flag = 0; // we want to talk to one process only at a time

int device_open(struct inode *_inode, struct file *_file)
{
    unsigned long flags = 0;
    spin_lock_irqsave(&device_lock, flags);
    if (dev_open_flag == 1)
    {
        printk(KERN_ERR "The device is busy\n");
        spin_unlock_irqrestore(&device_lock, flags);
        return -EBUSY;
    }

    int minor_number = iminor(_file);
    // check if we initialized a data structure for that mnior. otherwise initialize one.
    if (minors_to_channels == NULL)
    {
        initialize_minor_to_channels(MAX_MINORS);
    }
    if (get_channels_obj_of_minor(minor_number) == NULL)
    {
        add_minor(minor_number);
    }
    ++dev_open_flag;
    spin_unlock_irqrestore(&device_lock, flags);
    return 0;
}

int device_ioctal(struct file *_file, unsigned int control_command, unsigned long command_parameter)
{
    int minor_number = iminor(_file);
    // set current channel
    if (control_command == 0)
    {
        channels_t *_channels;
        if ((channels = get_channels_obj_of_minor(minors_to_channels)) == NULL)
        {
            printk("minor channels should be initialized\n");
            exit(1);
        }
        _channels->current_channel = command_parameter;
    }
}

int device_read(struct file *_file, char *buff, size_t *buff_size, loff_t *file_offset)
{
}

int device_write(struct file *_file, char *buff, size_t *buff_size, loff_t *file_offset)
{
}

void free_minors_data(minors_data_t *_minors_data)
{
}

int device_release()
{
    unsigned long flags = 0;
    spin_lock_irqsave(&device_lock, flags);
    free_minors_data(minors_data_t * _minors_data)-- dev_open_flag; // the driver is free to talk with another process
    spin_unlock_irqrestore(&device_lock, flags);
}

// DEVICE SETUP (as we saw in recitation 6 chardev 1)

struct file_operations Fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release,
};

// Initialize the module - Register the character device
static int __init simple_init(void)
{
    // init dev struct
    memset(&device_info, 0, sizeof(struct chardev_info));
    spin_lock_init(&device_lock);

    // Register driver capabilities. Obtain major num
    major = register_chrdev(0, DEVICE_RANGE_NAME, &Fops);

    // Negative values signify an error
    if (major < 0)
    {
        printk(KERN_ALERT "%s registraion failed for  %d\n",
               DEVICE_FILE_NAME, major);
        return major;
    }

    printk("Registeration is successful. "
           "The major device number is %d.\n",
           major);
    printk("If you want to talk to the device driver,\n");
    printk("you have to create a device file:\n");
    printk("mknod /dev/%s c %d 0\n", DEVICE_FILE_NAME, major);
    printk("You can echo/cat to/from the device file.\n");
    printk("Dont forget to rm the device file and "
           "rmmod when you're done\n");

    return 0;
}

static void __exit simple_cleanup(void)
{
    // Unregister the device
    // Should always succeed
    unregister_chrdev(major, DEVICE_RANGE_NAME);
}

module_init(simple_init);
module_exit(simple_cleanup);
