#include <linux/joystick.h>
#include <linux/kernel.h>

#define MAJOR_NO 145
#define BUF_LEN 100

static char msg[BUF_LEN] = { 0 };
static int msg_len = 0;

static ssize_t
piggy_read (struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
  ssize_t bytes_read;
  printk (KERN_INFO "%s called\n", __func__);

  if (*ppos >= msg_len)
    return 0;

  while (count && *ppos < msg_len)
    {
      if (copy_to_user (buffer++, &msg[*ppos], 1))
        return -EFAULT;
      count--;
      (*ppos)++;
      bytes_read++;
    }

  printk (KERN_INFO "Read %zu bytes\n", bytes_read);
  return bytes_read;
}

static ssize_t
piggy_write (struct file *file, const char __user *buffer, size_t count,
             loff_t *ppos)
{
  ssize_t bytes_written = 0;
  printk (KERN_INFO "%s called\n", __func__);

  if (*ppos >= BUF_LEN)
    return -ENOSPC;

  while (count && *ppos < BUF_LEN)
    {
      if (copy_from_user (&msg[*ppos], buffer++, 1))
        return -EFAULT;
      count--;
      (*ppos)++;
      bytes_written++;
    }

  msg_len = *ppos;

  printk (KERN_INFO "Wrote %zu bytes\n", bytes_written);
  return bytes_written;
}

static struct file_operations fops = {
  .read = piggy_read,
  .write = piggy_write,
  .open = NULL,
  .release = NULL,
};

static int __init
piggy_init (void)
{
  int major;

  printk (KERN_INFO "%s called\n", __func__);

  major = register_chrdev (MAJOR_NO, "piggy_dev", &fops);
  if (major < 0)
    printk (KERN_INFO "piggy_dev registration failed\n");
  else
    printk (KERN_INFO "piggy_dev registration successful with major no.: %d\n",
            major);

  return 0;
}

static void __exit
piggy_exit (void)
{
  printk (KERN_INFO "%s called\n", __func__);
  unregister_chrdev (MAJOR_NO, "piggy_dev");
}

module_exit (piggy_exit);
module_init (piggy_init);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("OS #10");
MODULE_DESCRIPTION ("USB driver for RedGear Gamepad");
MODULE_VERSION ("0.1");
