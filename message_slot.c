#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include "message_slot.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/ioctl.h>

MODULE_LICENSE("GPL");

/*Linked List imlemetation*/
struct Node
{
    int key;
    void *value;
    struct Node *next;
};

typedef struct LinkedList
{
    struct Node *head;
    struct Node *last;
    int length;
} LinkedList_t;

void initialize_lst(LinkedList_t **out);
void add_element(LinkedList_t *, int key, void *val);
void get_value(LinkedList_t *, int key, void ***out);
void exist_in_lst(LinkedList_t *, int key);

void initialize_lst(LinkedList_t **out)
{
    *out = kmalloc(sizeof(LinkedList_t));
    *out->head = NULL;
    *out->last = NULL;
    *out->length = 0;
}

void add_element(LinkedList_t *lst, int key, void *val)
{
    // create new node
    struct Node *node = kmalloc(sizeof(struct Node *));
    node->next = NULL;
    node->key = key;
    node->value = val;
    // place node in lst
    if (lst->head == NULL)
    {
        lst->head = node;
        lst->last = node;
    }
    else
    {
        lst->last->next = node;
        lst->last = node;
    }
}

void get_value(LinkedList_t *lst, int key, void ***out)
{
    struct Node *curr_node;
    curr_node = lst->head;
    *out = NULL;
    while (curr_node != NULL)
    {
        if (curr_node->key != key)
        {
            *out = &curr_node->value;
        }
        curr_node = curr_node->next;
    }
}

int exist_in_lst(LinkedList_t *lst, int key)
{
    struct Node *out;
    get_value(lst, key, &out);
    if (out == NULL)
        return 0;
    return 1;
}

/* define lock */
struct chardev_info
{
    spinlock_t lock;
};
// used to prevent concurent access into the same device
static int dev_open_flag = 0;
static struct chardev_info device_info;

/* define channels lst */
static LinkedList_t *channels_lst = NULL;
typedef struct channels
{
    int current_idx;
    LinkedList_t *messages;
} channels_t;

channels_t *create_channel(void)
{
    channels_t *channel = kmalloc(sizeof(channels_t));
    initialize_lst(&channel->messages);
    channel->current_idx = 0;
    return channel;
}

void update_message(channels_t *channel, int channel_idx, char *msg)
{
    char **msg_mem;
    if (exist_in_lst(channel->messages, channel_idx) == 1)
    {
        get_value(channel->messages, channel_idx, &msg_mem);
        *msg_mem = msg;
    }
    else
    {
        add_element(channel->messages, channel_idx, msg);
    }
}

static int device_open(struct inode *_inode, struct file *_file)
{
    int minor_number;
    unsigned long flags = 0;
    channels_t minor_channel;
    // lock stuff
    spin_lock_irqsave(&device_lock, flags);
    if (dev_open_flag == 1)
    {
        printk(KERN_ERR "The device is busy\n");
        spin_unlock_irqrestore(&device_lock, flags);
        return -EBUSY;
    }
    // save minor in struct file
    minor_number = iminor(_inode);
    _file->private_data = &minor_number;
    // initialize a data structure for mnior if needed.
    if (channels_lst == NULL)
    {
        initialize_lst(&channels_lst);
    }
    if ((exist_in_lst(channels_lst, minor_number)) == 0)
    {
        minor_channel = create_channel();
        add_element(channels_lst, minor_number, minor_channel);
    }
    ++dev_open_flag;
    spin_unlock_irqrestore(&device_lock, flags);
    return 0;
}

static long device_ioctal(struct file *_file, unsigned int ioctl_command_id, unsigned long ioctl_param)
{
    channels_t **_channels;
    int *minor_number_ptr, minor_number;
    minor_number_ptr = (int *)(_file->private_data);
    minor_number = *minor_number_ptr;

    if (IOCTL_SET_CAHNNEL_IDX == ioctl_command_id)
    {
        if (exist_in_lst(channels_lst, minor_number) == 0)
        {
            printk(KERN_ERR "ERROR in device ioctal. channel should be initialized.") return ERROR;
        }
        get_value(channels_lst, minor_number, &_channels);
        (*_channels)->current_idx = ioctl_param;
    }

    return SUCCESS;
}

static ssize_t device_read(struct file *_file, char __user *buffer, size_t length, loff_t *offset)
{
    char **msg;
    channels_t **_channels;
    int *minor_number_ptr, minor_number;
    minor_number_ptr = (int *)(_file->private_data);
    minor_number = *minor_number_ptr;

    //set current channel
    get_value(channels_lst, minor_number, &_channels);
    if (*_channels == NULL)
    {
        printk(KERN_ERR "minor channels should be initialized\n");
        return ERROR;
    }

    get_value((*_channels)->messages, (*_channels)->current_idx, &msg);
    for (i = 0; i < length; i++)
    {
        put_user((*msg)[i], &(buffer[i]));
    }

    return length;
}

static ssize_t device_write(struct file *_file, const char *buff, size_t buff_size, loff_t *file_offset)
{
    char *msg;
    channels_t **_channels;
    int *minor_number_ptr, minor_number;
    minor_number_ptr = (int *)(_file->private_data);
    minor_number = *minor_number_ptr;

    //set current channel
    get_value(channels_lst, minor_number, &_channels);
    if (*_channels == NULL)
    {
        printk(KERN_ERR "minor channels should be initialized\n");
        return ERROR;
    }

    for (i = 0; i < length; i++)
    {
        get_user((*msg)[i], &(buffer[i]));
    }
    add_element((*_channels)->messages, (*_channels)->current_idx, msg);

    return length;
}

void free_channels(channels_t *_channels)
{
    struct Node *curr_node = _channels->messages->head;
    struct Node *prev_node = NULL;
    while (curr_node != NULL)
    {
        if (prev_node != NULL)
        {
            kfree(prev_node->value);
            kfree(prev_node);
        }
        prev_node = curr_node;
        curr_node = curr_node->next;
    }
}
void free_channels_lst(LinkedList_t *lst)
{
    struct Node *curr_node = _channels->messages->head;
    struct Node *prev_node = NULL;
    while (curr_node != NULL)
    {
        if (prev_node != NULL)
        {
            free_channels(prev_node->value);
            kfree(prev_node->value);
            kfree(prev_node);
        }
        prev_node = curr_node;
        curr_node = curr_node->next;
    }
}

static int device_release(struct inode *_inode, struct file *_file)
{
    unsigned long flags = 0;
    _file->private_data = NULL;
    spin_lock_irqsave(&device_lock, flags);
    free_channels_lst(channels_lst);
    dev_open_flag--; // the driver is free to talk with another process
    spin_unlock_irqrestore(&device_lock, flags);
    return 0;
}

// device setup
struct file_operations Fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .unlocked_ioctl = device_ioctl,
    .release = device_release,
};

static int __init simple_init(void)
{
    int rc = -1;
    // init dev struct
    memset(&device_info, 0, sizeof(struct chardev_info));
    spin_lock_init(&device_info.lock);

    rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);

    // Negative values signify an error
    if (rc < 0)
    {
        printk(KERN_ALERT "%s registraion failed for  %d\n",
               DEVICE_FILE_NAME, MAJOR_NUM);
        return rc;
    }

    printk("Registeration is successful. ");
    return SUCCESS;
}

tatic void __exit simple_cleanup(void)
{
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

module_init(simple_init);
module_exit(simple_cleanup);
