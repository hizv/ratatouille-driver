/* remove redundancy */
#include <linux/bits.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/usb/input.h>
#include <linux/usb/quirks.h>

/* From the Linux kernel:
 * Wired RedGear devices have protocol 1,
 * wireless controllers have protocol 129.
 */
#define DEVICE_VENDOR_ID 0x045e
#define DEVICE_PRODUCT_ID 0x028e
#define DEVICE_MAJOR 93
#define DEVICE_MINOR_WIRED 1
#define DEVICE_MINOR_WIRELESS 129
// #define CHAR_DEV_NAME "readgear_dev"
#define RG_PKT_LEN 64

static struct usb_device_id rg_table[] = {
  { USB_DEVICE_AND_INTERFACE_INFO (DEVICE_VENDOR_ID, DEVICE_PRODUCT_ID,
                                   USB_CLASS_VENDOR_SPEC, DEVICE_MAJOR,
                                   DEVICE_MINOR_WIRED) },
  { USB_DEVICE_AND_INTERFACE_INFO (DEVICE_VENDOR_ID, DEVICE_PRODUCT_ID,
                                   USB_CLASS_VENDOR_SPEC, DEVICE_MAJOR,
                                   DEVICE_MINOR_WIRELESS) },
  {} /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, rg_table);

static const struct rg_device
{
  u16 vendor_id;
  u16 product_id;
  char *name;
  u8 packet_type;
} rg_device[] = {
  { DEVICE_VENDOR_ID, DEVICE_PRODUCT_ID,
    "Red Gear Gamepad (Compatible with Windows 7/8/8.1/10 only)" },
};

/* check for bloat */
struct rg_usb
{
  struct input_dev *dev;
  struct usb_device *udev;
  struct usb_interface *intf;

  bool input_created;

  struct urb *irq;
  unsigned char *data;
  dma_addr_t data_dma;

  int init_seq;

  int packet_type;
  char name[128];
  char phys[64];

  struct work_struct work;
};

static int rg_probe (struct usb_interface *intf,
                     const struct usb_device_id *id);
static void rg_disconnect (struct usb_interface *interface);
// static ssize_t rg_read (struct file *file, char __user *buffer, size_t
// count,
//                         loff_t *ppos);
// static int rg_open(struct input_dev *dev);

// static struct file_operations rg_fops = {
//   .owner = THIS_MODULE,
//   .read = rg_read,
//   .write = NULL,
//   .open = NULL,
//   .release = NULL,
// };

static struct usb_driver rg_driver = {
  .name = "readgear",
  .probe = rg_probe,
  .disconnect = rg_disconnect,
  .id_table = rg_table,
};

static const signed short rg_common_btn[] = {
  BTN_A,     BTN_B,      BTN_X,      BTN_Y,      /* "analog" buttons */
  BTN_START, BTN_SELECT, BTN_THUMBL, BTN_THUMBR, /* start/back/sticks */
  -1                                             /* terminating entry */
};

/* the meat of the project, process incoming packets */
static void
process_packet (struct rg_usb *rg, struct input_dev *dev, unsigned char *data)
{
  printk (KERN_INFO "processing packet, data[0]: %d\n", data[0]);

  /* valid pad data */
  if (data[0] != 0x00)
    return;

  /* start/back buttons */
  input_report_key (dev, BTN_START, data[2] & BIT (4));
  input_report_key (dev, BTN_SELECT, data[2] & BIT (5));

  /* stick press left/right */
  input_report_key (dev, BTN_THUMBL, data[2] & BIT (6));
  input_report_key (dev, BTN_THUMBR, data[2] & BIT (7));

  /* buttons A,B,X,Y,TL,TR and MODE */
  input_report_key (dev, BTN_A, data[3] & BIT (4));
  input_report_key (dev, BTN_B, data[3] & BIT (5));
  input_report_key (dev, BTN_X, data[3] & BIT (6));
  input_report_key (dev, BTN_Y, data[3] & BIT (7));

  /* print the data values */
  printk (KERN_ALERT "data[0] = %d\n", data[0]);
  printk (KERN_ALERT "data[1] = %d\n", data[1]);
  printk (KERN_ALERT "data[2] = %d\n", data[2]);
  printk (KERN_ALERT "data[3] = %d\n", data[3]);

  input_sync (dev);
}

/* urb initialization complete callback fn */
static void
rg_irq_in (struct urb *urb)
{
  struct rg_usb *rg = urb->context;
  int retval;

  printk (KERN_INFO "urb callback with status: %d\n", urb->status);

  switch (urb->status)
    {
    case 0:
      /* success */
      break;
    case -ECONNRESET:
    case -ENOENT:
    case -ESHUTDOWN:
      /* this urb is terminated, clean up */
      printk (KERN_ALERT "%s - urb shutting down with status: %d\n", __func__,
              urb->status);
      return;
    default:
      printk (KERN_ALERT "%s - nonzero urb status received: %d\n", __func__,
              urb->status);
      retval = usb_submit_urb (urb, GFP_ATOMIC);
      if (retval)
        printk (KERN_ALERT "%s - usb_submit_urb failed with result %d\n",
                __func__, retval);
      return;
    }

  process_packet (rg, rg->dev, rg->data);
}

/* called when usb device is plugged in */
static int
rg_probe (struct usb_interface *intf, const struct usb_device_id *id)
{
  struct usb_device *udev = interface_to_usbdev (intf);
  struct rg_usb *rg;
  struct usb_endpoint_descriptor *ep_irq_in;
  int i, error;

  if (intf->cur_altsetting->desc.bNumEndpoints != 2)
    return -ENODEV;

  if (le16_to_cpu (udev->descriptor.idVendor) != DEVICE_VENDOR_ID
      || le16_to_cpu (udev->descriptor.idProduct) != DEVICE_PRODUCT_ID)
    return -ENODEV;

  rg = kzalloc (sizeof (struct rg_usb), GFP_KERNEL);
  if (!rg)
    return -ENOMEM;

  usb_make_path (udev, rg->phys, sizeof (rg->phys));
  strlcat (rg->phys, "/input0", sizeof (rg->phys));
  printk (KERN_INFO "phys: %s\n", rg->phys);

  /* purpose not known */
  rg->data = usb_alloc_coherent (udev, RG_PKT_LEN, GFP_KERNEL, &rg->data_dma);
  if (!rg->data)
    {
      error = -ENOMEM;
      goto rg_err_free_mem;
    }

  /* 0 for interrupts */
  rg->irq = usb_alloc_urb (0, GFP_KERNEL);
  if (!rg->irq)
    {
      error = -ENOMEM;
      goto rg_err_free_data;
    }

  rg->udev = udev;
  rg->intf = intf;
  strncpy (rg->name,
           "Red Gear Pro Gamepad (Compatible with Windows 7/8/8.1/10 only)",
           sizeof (rg->name));
  rg->packet_type = 2; // PKT_XBE2_FW_OLD

  /* get the IN interrupt for endpoint */
  ep_irq_in = NULL;

  for (i = 0; i < 2; i++)
    {
      struct usb_endpoint_descriptor *ep
          = &intf->cur_altsetting->endpoint[i].desc;

      if (usb_endpoint_xfer_int (ep) && usb_endpoint_dir_in (ep))
        ep_irq_in = ep;
    }

  if (!ep_irq_in)
    {
      error = -ENODEV;
      goto rg_err_free_in_urb;
    }

  usb_fill_int_urb (rg->irq, udev,
                    usb_rcvintpipe (udev, ep_irq_in->bEndpointAddress),
                    rg->data, RG_PKT_LEN, rg_irq_in, rg, ep_irq_in->bInterval);
  rg->irq->transfer_dma = rg->data_dma;
  rg->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

  usb_set_intfdata (intf, rg);

  // reg_return = register_chrdev (DEVICE_MAJOR, CHAR_DEV_NAME, &rg_fops);
  // if (reg_return < 0)
  //   printk (KERN_INFO CHAR_DEV_NAME " registration failed\n");
  // else
  //   printk (KERN_INFO CHAR_DEV_NAME " registration successful\n");

  // return reg_return;

rg_err_free_in_urb:
  usb_free_urb (rg->irq);
rg_err_free_data:
  usb_free_coherent (udev, RG_PKT_LEN, rg->data, rg->data_dma);
rg_err_free_mem:
  kfree (rg);
  return error;
}

static void
rg_disconnect (struct usb_interface *intf)
{
  struct rg_usb *rg = usb_get_intfdata (intf);

  // rg_deinit_input(rg);

  usb_free_urb (rg->irq);
  usb_free_coherent (rg->udev, RG_PKT_LEN, rg->data, rg->data_dma);

  kfree (rg);

  usb_set_intfdata (intf, NULL);
}

// static ssize_t
// rg_read (struct file *file, char __user *buffer, size_t count, loff_t *ppos)
// {
//   printk (KERN_INFO "%s called\n", __func__);
//   return 0;
// }

module_usb_driver (rg_driver);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("OS #10");
MODULE_DESCRIPTION ("USB driver for RedGear Gamepad");
MODULE_VERSION ("0.1");
