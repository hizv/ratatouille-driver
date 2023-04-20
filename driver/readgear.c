#include <linux/kernel.h>
#include <linux/usb/input.h>

/* From the Linux kernel:
 * Wired RedGear devices have protocol 1,
 * wireless controllers have protocol 129.
 */
#define DEVICE_MAJOR 93
#define DEVICE_MINOR_WIRED 1
#define DEVICE_MINOR_WIRELESS 129
#define CHAR_DEV_NAME "readgear_dev"

static struct usb_device_id rg_table[] = {
  { USB_DEVICE_AND_INTERFACE_INFO (0x045e, 0x028e, USB_CLASS_VENDOR_SPEC,
                                   DEVICE_MAJOR, DEVICE_MINOR_WIRED) },
  { USB_DEVICE_AND_INTERFACE_INFO (0x045e, 0x028e, USB_CLASS_VENDOR_SPEC,
                                   DEVICE_MAJOR, DEVICE_MINOR_WIRELESS) },
  {} /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, rg_table);

static int rg_probe (struct usb_interface *intf,
                     const struct usb_device_id *id);
static void rg_disconnect (struct usb_interface *interface);
static ssize_t rg_read (struct file *file, char __user *buffer, size_t count,
                        loff_t *ppos);
// static int rg_open(struct input_dev *dev);

static struct file_operations rg_fops = {
  .owner = THIS_MODULE,
  .read = rg_read,
  .write = NULL,
  .open = NULL,
  .release = NULL,
};

static struct usb_driver rg_driver = {
  .name = "readgear",
  .probe = rg_probe,
  .disconnect = rg_disconnect,
  .id_table = rg_table,
};

static int
rg_probe (struct usb_interface *intf, const struct usb_device_id *id)
{
  int status;

  status = register_chrdev (DEVICE_MAJOR, CHAR_DEV_NAME, &rg_fops);
  if (status < 0)
    printk (KERN_INFO CHAR_DEV_NAME " registration failed\n");
  else
    printk (KERN_INFO CHAR_DEV_NAME " registration successful\n");

  return status;
}

static void
rg_disconnect (struct usb_interface *interface)
{
  printk (KERN_INFO "%s called\n", __func__);
  unregister_chrdev (DEVICE_MAJOR, CHAR_DEV_NAME);
}

static ssize_t
rg_read (struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
  printk (KERN_INFO "%s called\n", __func__);
  return 0;
}

// static int rg_open(struct input_dev *dev)
// {
//   printk(KERN_INFO "%s called\n", __func__);
//   return 0;
// }

static int __init
rg_init (void)
{
  int result;

  printk (KERN_INFO "%s called\n", __func__);

  result = usb_register (&rg_driver);
  if (result < 0)
    {
      printk (KERN_ALERT "Error: %d\n Module registration failed\n", result);
      return -1;
    }

  return 0;
}

static void __exit
rg_exit (void)
{
  printk (KERN_INFO "%s called\n", __func__);
  usb_deregister (&rg_driver);
}

module_exit (rg_exit);
module_init (rg_init);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("OS #10");
MODULE_DESCRIPTION ("USB driver for RedGear Gamepad");
MODULE_VERSION ("0.1");
