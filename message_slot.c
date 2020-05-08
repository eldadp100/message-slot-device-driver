/*
    This file implemets the message slot IPC driver.
    To add (register) the module to the kernel use:
    To remove the module: 
*/

// TODO: don't limit number of channels and minor numbers

#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>

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
    simple_dict_t *dict = vmalloc(sizeof(simple_dict_t));
    dict->entries = vmalloc(max_number_of_keys * sizeof(int) + sizeof_value);
    dict->current_entry_index = 0;
    dict->max_etries = max_number_of_keys;
    return dict;
}

void *get_value(simple_dict_t *dict, int key)
{
    int i;
    for (i = 0; i < dict->current_entry_index; i++)
    {
        if (key == dict->entries[i].key)
            return &dict->entries[i].val;
    }
    return NULL;
}

void set_value(simple_dict_t *dict, int key, void *value)
{
    if (dict->current_entry_index == dict->max_etries - 1)
    {
        printk(KERN_ERR "simple dict is out of bounds.\n");
        //exit(1);
    }
    dict->entries[dict->current_entry_index].key = key;
    dict->entries[dict->current_entry_index].val = value;
    dict->current_entry_index++;
}

typedef struct channels
{
    int current_channel;
    char **messages;
} channels_t;

channels_t *create_new_channel(void)
{
    int i;
    channels_t *new_channels = vmalloc(sizeof(channels_t));
    new_channels->messages = vmalloc(CHANNELS_NUM * sizeof(char *));
    for (i = 0; i < CHANNELS_NUM; i++)
    {
        new_channels->messages[i] = NULL;
    }
    new_channels->current_channel = 0;
    return new_channels;
}

simple_dict_t *minors_to_channels = NULL;

void initialize_minor_to_channels(int number_of_minors)
{
    minors_to_channels = create_simple_dict(number_of_minors, sizeof(channels_t));
}

channels_t *get_channels_obj_of_minor(int minor_num)
{

    if (minors_to_channels == NULL)
    {
        printk(KERN_ERR "minors_to_channels object is not initialized\n");
        //exit(1);
    }
    return (channels_t*)get_value(minors_to_channels, minor_num);
}

void add_minor(int minor_num)
{
    channels_t *new_channel = create_new_channel();
    set_value(minors_to_channels, minor_num, new_channel);
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

    int minor_number = iminor(_inode);
    _file->private_data = minor_number;
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
    int minor_number = _file->private_data;
    // set current channel
    if (control_command == 0)
    {
        channels_t *_channels;
        if ((_channels = get_channels_obj_of_minor(minor_number)) == NULL)
        {
            printk("minor channels should be initialized\n");
            //exit(1);
        }
        _channels->current_channel = command_parameter;
    }
    return 0;
}

int device_read(struct file *_file, char *buff, size_t buff_size, loff_t *file_offset)
{
    int i, minor_number;
    char *msg;
    minor_number = _file->private_data;
    // set current channel
    channels_t *_channels;
    if ((_channels = get_channels_obj_of_minor(minor_number)) == NULL)
    {
        printk(KERN_ERR "minor channels should be initialized\n");
        //exit(1);
    }

    msg = _channels->messages[_channels->current_channel];
    for (i = 0; i < buff_size; i++)
    {
        put_user(msg[i], &buff[i]);
    }
    return 0;
}

int device_write(struct file *_file, char *buff, size_t buff_size, loff_t *file_offset)
{
    int i, minor_number;
    char *msg = vmalloc(buff_size);
    minor_number = _file->private_data;
    // set current channel
    channels_t *_channels;
    if ((_channels = get_channels_obj_of_minor(minor_number)) == NULL)
    {
        printk(KERN_ERR "minor channels should be initialized\n");
        //exit(1);
    }

    for (i = 0; i < buff_size; i++)
    {
        get_user(msg[i], &buff[i]);
    }
    _channels->messages[_channels->current_channel] = msg;
    return 0;
}

void free_channels(channels_t *_channels)
{
    int i;
    for (i = 0; i < CHANNELS_NUM; i++)
    {
        kfree(_channels->messages[i]);
    }
}
void free_minors_data(simple_dict_t *dict)
{
    int i;
    for (i = 0; i < dict->current_entry_index; i++)
    {
        free_channels(dict->entries[i].val);
        kfree(&dict->entries[i]);
    }
    kfree(dict);
}

int device_release()
{
    unsigned long flags = 0;
    spin_lock_irqsave(&device_lock, flags);
    free_minors_data(minors_to_channels);
    dev_open_flag--; // the driver is free to talk with another process
    spin_unlock_irqrestore(&device_lock, flags);
    return 0;
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
