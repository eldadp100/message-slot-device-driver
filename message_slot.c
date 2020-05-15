
/*
    Message Slot is a list of slots. slot for each minor.
    each slot is a list of channels
    each channel is a message buffer
*/

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

LinkedList_t *initialize_lst(void)
{
    LinkedList_t *out;
    out = kmalloc(sizeof(LinkedList_t), GFP_KERNEL);
    out->head = NULL;
    out->last = NULL;
    out->length = 0;
    return out;
}

void add_element(LinkedList_t *lst, int key, void *val)
{
    // create new node in heap
    struct Node *node = kmalloc(sizeof(struct Node *), GFP_KERNEL);
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

struct Node *get_node(LinkedList_t *lst, int key)
{
    struct Node *curr_node;
    curr_node = lst->head;
    while (curr_node != NULL)
    {
        if (curr_node->key != key)
        {
            return curr_node;
        }
        curr_node = curr_node->next;
    }
    return NULL;
}

void *get_value(LinkedList_t *lst, int key)
{
    struct Node *node;
    node = get_node(lst, key);
    return node->value;
}

void change_value(LinkedList_t *lst, int key, void *new_val)
{
    struct Node *node;
    node = get_node(lst, key);
    node->value = new_val;
}

int exist_in_lst(LinkedList_t *lst, int key)
{
    struct Node *node;
    node = get_node(lst, key);
    if (node == NULL)
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
static LinkedList_t *global_slots_lst = NULL;
typedef struct slot
{
    int current_channel;
    LinkedList_t *channels;
} slot_t;

typedef struct channel
{
    char *message;
} channel_t;

slot_t *create_slot(void)
{
    slot_t *slot = kmalloc(sizeof(slot_t), GFP_KERNEL);
    slot->channels = initialize_lst();
    slot->current_channel = 0;
    return slot;
}

channel_t* create_channel(void)
{
    channel_t *channel = kmalloc(sizeof(channel_t), GFP_KERNEL);
    return channel;
}

int update_message(LinkedList_t *slots_lst, int minor_number, char *new_msg)
{
    slot_t *minor_slot;
    channel_t *channel;
    int channel_number;
    if (exist_in_lst(slots_lst, minor_number) == 0)
    {
        printk(KERN_ERR "ERROR in message write (update). slot should be initialized.");
        return ERROR;
    }
    else
    {
        minor_slot = (slot_t *)get_value(slots_lst, minor_number);
    }
    channel_number = minor_slot->current_channel;
    if (exist_in_lst(minor_slot->channels, channel_number) == 0)
    {
        channel = create_channel();
        channel->message = new_msg;
        add_element(minor_slot->channels, channel_number, channel);
    }
    else
    {
        channel = (channel_t *)get_value(minor_slot->channels, channel_number);
        channel->message = new_msg;
    }
    return SUCCESS;
}



char* read_message(LinkedList_t *slots_lst, int minor_number)
{
    slot_t *minor_slot;
    channel_t *channel;
    int channel_number;
    if (exist_in_lst(slots_lst, minor_number) == 0)
    {
        printk(KERN_ERR "ERROR in message read. slot should be initialized.");
        return NULL;
    }
    else
    {
        minor_slot = (slot_t *)get_value(slots_lst, minor_number);
    }
    channel_number = minor_slot->current_channel;
    if (exist_in_lst(minor_slot->channels, channel_number) == 0)
    {
        printk(KERN_ERR "ERROR in message read. channel should be initialized.");
        return NULL;
    }
    else
    {
        channel = (channel_t *)get_value(minor_slot->channels, channel_number);
        return channel->message;
    }
}

static int device_open(struct inode *_inode, struct file *_file)
{
    int minor_number;
    unsigned long flags;
    slot_t *minor_slot;
    // lock stuff
    spin_lock_irqsave(&device_info.lock, flags);
    if(1 == dev_open_flag)
    {
        spin_unlock_irqrestore(&device_info.lock, flags);
        return -EBUSY;
    }
    // save minor in struct file
    minor_number = iminor(_inode);
    _file->private_data = &minor_number;
    // initialize a data structure for mnior if needed.
    if (global_slots_lst == NULL)
    {
        global_slots_lst = initialize_lst();
    }
    if (exist_in_lst(global_slots_lst, minor_number) == 0)
    {
        minor_slot = create_slot();
        add_element(global_slots_lst, minor_number, minor_slot);
    }
    ++dev_open_flag;
    spin_unlock_irqrestore(&device_info.lock, flags);
    return SUCCESS;
}

static long device_ioctal(struct file *_file, unsigned int ioctl_command_id, unsigned long ioctl_param)
{
    slot_t *minor_slot;
    int *minor_number_ptr, minor_number;
    minor_number_ptr = (int *)(_file->private_data);
    minor_number = *minor_number_ptr;

    if (IOCTL_SET_CAHNNEL_IDX == ioctl_command_id)
    {
        if (exist_in_lst(global_slots_lst, minor_number) == 0)
        {
            printk(KERN_ERR "ERROR in device ioctal. slot should be initialized.");
            return ERROR;
        }
        minor_slot = get_value(global_slots_lst, minor_number);
        minor_slot->current_channel = ioctl_param;
    }

    return SUCCESS;
}

static ssize_t device_read(struct file *_file, char __user *buffer, size_t length, loff_t *offset)
{
    char *msg;
    int *minor_number_ptr, minor_number, i;
    minor_number_ptr = (int *)(_file->private_data);
    minor_number = *minor_number_ptr;

    msg = read_message(global_slots_lst, minor_number);
    if (msg == NULL)
    {
        return ERROR;
    }
    
    for (i = 0; i < length; i++)
    {
        put_user(msg[i], &(buffer[i]));
    }

    return length;
}

static ssize_t device_write(struct file *_file, const char __user *buffer, size_t length, loff_t *offset)
{
    char *msg;
    int *minor_number_ptr, minor_number, ret, i;
    minor_number_ptr = (int *)(_file->private_data);
    minor_number = *minor_number_ptr;
    msg = kmalloc(length, GFP_KERNEL);
    for (i = 0; i < length; i++)
    {
        get_user(msg[i], &(buffer[i]));
    }
    
    ret = update_message(global_slots_lst, minor_number, msg);
    if (ret == ERROR)
    {
        return 0;
    }
    return length;
}

void free_slot(slot_t *slot)
{
    struct Node *curr_node = slot->channels->head;
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
    kfree(slot);
}
void free_slot_lst(LinkedList_t *slot_lst)
{
    struct Node *curr_node = slot_lst->head;
    struct Node *prev_node = NULL;
    while (curr_node != NULL)
    {
        if (prev_node != NULL)
        {
            free_slot(prev_node->value);
            kfree(prev_node);
        }
        prev_node = curr_node;
        curr_node = curr_node->next;
    }
}

static int device_release(struct inode *_inode, struct file *_file)
{
    unsigned long flags;
    spin_lock_irqsave(&device_info.lock, flags);
    // free memory
    // _file->private_data = NULL;
    // free_slot_lst(global_slots_lst);
    // ready for our next caller
    --dev_open_flag;
    spin_unlock_irqrestore(&device_info.lock, flags);
    return SUCCESS;
}

// device setup
struct file_operations Fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .unlocked_ioctl = device_ioctal,
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

static void __exit simple_cleanup(void)
{
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

module_init(simple_init);
module_exit(simple_cleanup);
