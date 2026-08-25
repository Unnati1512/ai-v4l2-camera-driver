#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#define DEVICE_NAME "mychardev"

static int major_number;
static int is_open_flag = 0;
static char message[256] = "Hello from the kernel character device!\n";
static int message_len = 0;

static int dev_open(struct inode *inodep, struct file *filep)
{
  if(is_open_flag)
     return -EBUSY;
  is_open_flag = 1;
  message_len = strlen(message);
  printk(KERN_INFO "mychardev: device opened\n");
  return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
  int error_count = 0;
  if(message_len == 0)
     return 0;

  error_count = copy_to_user(buffer, message, message_len);

  if(error_count == 0) {
     printk(KERN_INFO "mychardev: sent %d characters to user\n", message_len);
     int sent = message_len;
     message_len = 0;
     return sent;
   } else {
         printk(KERN_INFO "mychardev: failed to send %d characters\n", error_count);
         return -EFAULT;
   }
}

static int dev_release(struct inode *inodep, struct file *filep)
{
  is_open_flag = 0;
  printk(KERN_INFO "mychardev: device closed\n");
  return 0;
}

static struct file_operations fops = {
  .open = dev_open,
  .read = dev_read,
  .release = dev_release,
};

static int __init chrdev_init(void)
{
  major_number = register_chrdev(0, DEVICE_NAME, &fops);
  if(major_number < 0){
     printk(KERN_ALERT "mychardev: failed to register a major number\n");
     return major_number;
 }
  printk(KERN_INFO "mychardev: registered with major number %d\n", major_number);
  printk(KERN_INFO "mychardev: create device node with: sudo mknod/dev/%s c %d 0\n", DEVICE_NAME, major_number);
  return 0;
}

static void __exit chrdev_exit(void)
{
  unregister_chrdev(major_number, DEVICE_NAME);
  printk(KERN_INFO "mychardev: unregistered\n");
}

module_init(chrdev_init);
module_exit(chrdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("UNNATI");
MODULE_DESCRIPTION("Day 2- Basic character device driver");
 
