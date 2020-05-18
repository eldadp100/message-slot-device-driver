
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
    struct Node *node = kmalloc(sizeof(struct Node), GFP_KERNEL);
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
        if (curr_node->key == key)
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

void print_linked_list(LinkedList_t *lst)
{
    struct Node *curr_node;
    // printk(KERN_DEBUG "START PRINT LIST\n");
    // printk(KERN_DEBUG "lst address %lu \n", (unsigned long)lst);
    curr_node = lst->head;
    while (curr_node != NULL)
    {
        printk(KERN_DEBUG "%d, ", curr_node->key);
        curr_node = curr_node->next;
    }
    // printk(KERN_DEBUG "\nSTOP PRINT LIST\n");
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
    LinkedList_t *message;
    int message_size;
} channel_t;

slot_t *create_slot(void)
{
    slot_t *slot = kmalloc(sizeof(slot_t), GFP_KERNEL);
    slot->channels = initialize_lst();
    slot->current_channel = 0;
    return slot;
}

channel_t *create_channel(void)
{
    channel_t *channel = kmalloc(sizeof(channel_t), GFP_KERNEL);
    channel->message = initialize_lst();
    channel->message_size = 0;
    return channel;
}

int update_message(LinkedList_t *slots_lst, int minor_number, char *new_msg, int message_size)
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
        add_element(minor_slot->channels, channel_number, channel);
    }
    else
    {
        channel = (channel_t *)get_value(minor_slot->channels, channel_number);
    }
    add_element(channel->message, message_size, new_msg);
    channel->message_size += message_size;
    return SUCCESS;
}

char *get_message_of_channel(channel_t *channel)
{
    char *concated_msg;
    struct Node *curr_node;
    char *curr_msg;
    int concated_msg_index = 0, j;
    concated_msg = kmalloc(channel->message_size, GFP_KERNEL);
    curr_node = channel->message->head;
    while (curr_node != NULL)
    {
        curr_msg = (char *)curr_node->value;
        for (j = 0; j < curr_node->key; j++)
        {
            concated_msg[concated_msg_index + j] = curr_msg[j];
        }
        concated_msg_index += curr_node->key;
        curr_node = curr_node->next;
    }

    if (concated_msg_index != channel->message_size)
        printk(KERN_ERR "ERROR IN get_message_from_message_lst.\nlast_idx=%id, message_size=%d\n", concated_msg_index, channel->message_size);
    return concated_msg;
}

char *read_message(LinkedList_t *slots_lst, int minor_number, int *out_msg_size)
{
    slot_t *minor_slot;
    channel_t *channel;
    int channel_number;
    char *concatenated_msg;
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
        concatenated_msg = get_message_of_channel(channel);
        *out_msg_size = channel->message_size;
        return concatenated_msg;
    }
}

static int device_open(struct inode *_inode, struct file *_file)
{
    unsigned int *minor_number;
    unsigned long flags;
    slot_t *minor_slot;
    // printk(KERN_DEBUG "OPEN INVOKED\n");
    // lock stuff
    spin_lock_irqsave(&device_info.lock, flags);
    if (1 == dev_open_flag)
    {
        spin_unlock_irqrestore(&device_info.lock, flags);
        return -EBUSY;
    }
    // save minor in struct file
    minor_number = (unsigned int *)kmalloc(sizeof(unsigned int), GFP_KERNEL);
    *minor_number = iminor(_inode);
    _file->private_data = minor_number;

    // initialize a data structure for mnior if needed.
    if (global_slots_lst == NULL)
    {
        global_slots_lst = initialize_lst();
    }
    if (exist_in_lst(global_slots_lst, *minor_number) == 0)
    {
        minor_slot = create_slot();
        add_element(global_slots_lst, *minor_number, minor_slot);
    }

    if (exist_in_lst(global_slots_lst, *minor_number) == 0)
    {
        printk(KERN_ERR "THERE IS A BUG IN LINKED LIST IMPLEMENTATION");
    }
    ++dev_open_flag;
    spin_unlock_irqrestore(&device_info.lock, flags);

    // printk(KERN_DEBUG "MINOR NUMBER: %d\n", *minor_number);
    // printk(KERN_DEBUG "OPEN SUCCEED\n");
    return SUCCESS;
}

static long device_ioctal(struct file *_file, unsigned int ioctl_command_id, unsigned long ioctl_param)
{

    slot_t *minor_slot;
    int *minor_number_ptr, minor_number;
    minor_number_ptr = (int *)(_file->private_data);
    minor_number = *minor_number_ptr;
    // printk(KERN_DEBUG "IOCTL INVOKED.\n change channel to: %lu. \nminor is: %d\n", ioctl_param, minor_number);
    // print_linked_list(global_slots_lst);

    if (IOCTL_SET_CAHNNEL_IDX == ioctl_command_id)
    {
        // printk(KERN_DEBUG "Check if minor slot exists. MINOR = %d", minor_number);
        if (exist_in_lst(global_slots_lst, minor_number) == 0)
        {
            printk(KERN_ERR "ERROR in device ioctal. slot should be initialized.");
            return ERROR;
        }
        minor_slot = get_value(global_slots_lst, minor_number);
        minor_slot->current_channel = ioctl_param;
    }

    // printk(KERN_DEBUG "IOCTL SUCCEEDED\n");
    return SUCCESS;
}

static ssize_t device_read(struct file *_file, char __user *buffer, size_t buff_length, loff_t *offset)
{
    char *msg;
    int *minor_number_ptr, minor_number, i;
    int total_msg_size, num_bytes_to_read;
    minor_number_ptr = (int *)(_file->private_data);
    minor_number = *minor_number_ptr;
    // printk(KERN_DEBUG "READ INVOKED.\n minor is: %ul\n", minor_number);
    // print_linked_list(global_slots_lst);

    msg = read_message(global_slots_lst, minor_number, &total_msg_size);
    if (msg == NULL)
        return -ERROR;

    num_bytes_to_read = buff_length;
    if (*offset + num_bytes_to_read > total_msg_size)
        num_bytes_to_read = total_msg_size - *offset;

    for (i = 0; i < num_bytes_to_read; i++)
    {
        put_user(msg[i + *offset], &(buffer[i]));
    }

    // printk(KERN_DEBUG "READ SUCCED");
    kfree(msg);
    *offset += num_bytes_to_read; //update offset
    return num_bytes_to_read;
}

static ssize_t device_write(struct file *_file, const char __user *buffer, size_t buff_length, loff_t *offset)
{
    char *msg;
    int *minor_number_ptr, minor_number, ret, i;
    minor_number_ptr = (int *)(_file->private_data);
    minor_number = *minor_number_ptr;
    msg = kmalloc(buff_length, GFP_KERNEL);

    // printk(KERN_DEBUG "WRITE INVOKED.\n minor is: %ul\n", minor_number);
    // print_linked_list(global_slots_lst);

    for (i = *offset; i < *offset + buff_length; i++)
    {
        get_user(msg[i], &(buffer[i]));
    }

    ret = update_message(global_slots_lst, minor_number, msg, buff_length);
    if (ret == ERROR)
    {
        return 0;
    }
    // printk(KERN_DEBUG "WRITE SUCCED");
    *offset += buff_length; //update offset
    return buff_length;
}

static int device_release(struct inode *_inode, struct file *_file)
{
    unsigned long flags;
    kfree(_file->private_data);

    // printk(KERN_DEBUG "MINOR DEVICE RELEASE.\n minor is: %d", iminor(_inode));
    spin_lock_irqsave(&device_info.lock, flags);
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

    printk(KERN_DEBUG "Registeration is successful. \n");
    return SUCCESS;
}

void free_channel(channel_t *channel)
{
    struct Node *curr_node, *prev_node;
    curr_node = channel->message->head;
    prev_node = NULL;
    while (curr_node != NULL)
    {
        if (prev_node != NULL)
        {
            kfree((char *)prev_node->value);
            kfree(prev_node);
        }
        prev_node = curr_node;
        curr_node = curr_node->next;
    }
    kfree(channel);
}

void free_slot(slot_t *slot)
{
    struct Node *curr_node, *prev_node;
    curr_node = slot->channels->head;
    prev_node = NULL;
    while (curr_node != NULL)
    {
        if (prev_node != NULL)
        {
            free_channel((channel_t *)prev_node->value);
            kfree(prev_node);
        }
        prev_node = curr_node;
        curr_node = curr_node->next;
    }
    kfree(slot);
}

void free_slot_lst(LinkedList_t *slot_lst)
{
    struct Node *curr_node, *prev_node;
    curr_node = slot_lst->head;
    prev_node = NULL;
    while (curr_node != NULL)
    {
        if (prev_node != NULL)
        {
            free_slot((slot_t *)prev_node->value);
            kfree(prev_node);
        }
        prev_node = curr_node;
        curr_node = curr_node->next;
    }
}

static void __exit simple_cleanup(void)
{
    // free memory
    free_slot_lst(global_slots_lst);
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

module_init(simple_init);
module_exit(simple_cleanup);
