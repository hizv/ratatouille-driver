#include <linux/joystick.h>
#include <linux/kernel.h>
#include <linux/module.h>

static int __init
piggy_init (void)
{
  printk (KERN_INFO "%s called\n", __func__);

  return 0;
}

static void __exit
piggy_exit (void)
{
  printk (KERN_INFO "%s called\n", __func__);
}

module_exit (piggy_exit);
module_init (piggy_init);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("OS #10");
MODULE_DESCRIPTION ("USB driver for RedGear Gamepad");
MODULE_VERSION ("0.1");
