#include <linux/fs.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include "camera_client.h"
#include <linux/gpio/consumer.h>

#define GPIO_BASE                 571
#define LED_RED_GPIO        (GPIO_BASE + 21)
#define LED_GREEN_GPIO      (GPIO_BASE + 20)

MODULE_LICENSE("GPL"); //*
MODULE_AUTHOR("Hajj Smirky"); //!
MODULE_DESCRIPTION("Wrapper driver for Camera Streaming"); //!

static int major;
static struct class *cls;
static struct device *dev;

static bool gpio_ready = false;
static struct gpio_desc *red_led, *green_led;

// open() -> Invoked when user-space process opens /dev/cam_stream
// Purpose: Log that /dev/cam_stream has been opened
static int cam_stream_open(struct inode *inode, struct file *file) 
{
    printk(KERN_INFO "cam_stream open: /dev/cam_stream opened by user-space\n");
    return 0;
}

// release() -> Invoked when user space process calls close(fd)
// Purpose: Close the device (/dev/cam_stream) opened in open()
static int cam_stream_release(struct inode *inode, struct file *file) 
{
    printk(KERN_INFO "cam_stream release: /dev/cam_stream released by user-space\n");
    return 0;
}

// ioctl() -> Invoked when user-space process issues an ioctl() on /dev/cam_stream
// Purpose:
//      - Handle custom LED control commands (CAM_IOC_START, CAM_IOC_STOP, CAM_IOC_RESET)
//      - Toggle GPIO LEDs to indicate camera status (RED, GREEN, YELLOW)
static long cam_stream_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
        case CAM_IOC_START:
            printk(KERN_INFO "cam_stream ioctl: START command received\n");
            if (gpio_ready) {
                gpiod_set_value(red_led, 1);        // Turn off RED
                gpiod_set_value(green_led, 0);      // Turn on GREEN
                printk(KERN_INFO "cam_stream ioctl: LED is GREEN\n");
            } else {
                printk(KERN_INFO "cam_stream ioctl: LED is GREEN (simulated)\n");
            }
            break;
        case CAM_IOC_STOP:
            printk(KERN_INFO "cam_stream ioctl: STOP command received\n");
            if (gpio_ready) {
                gpiod_set_value(green_led, 1);
                gpiod_set_value(red_led, 0);
                printk(KERN_INFO "cam_stream ioctl: LED is RED\n\n");
            } else {
                printk(KERN_INFO "cam_stream ioctl: LED is RED (simulated)\n");
            }
            break;
        case CAM_IOC_RESET:
            printk(KERN_INFO "cam_stream ioctl: RESET command received\n");
            if (gpio_ready) {
                gpiod_set_value(green_led, 0);      // Turn both RED and GREEN on
                gpiod_set_value(red_led, 0);
                printk(KERN_INFO "cam_stream ioctl: LED is YELLOW\n");
            } else {
                printk(KERN_INFO "cam_stream ioctl: LED is YELLOW - RESET (simulated)\n");
            }
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = cam_stream_open,
    .release = cam_stream_release,
    .unlocked_ioctl = cam_stream_ioctl,
};

static int __init my_init(void) 
{
    int status_red, status_green;

    printk(KERN_INFO "cam_stream init: Initializing\n");

    // 1. Allocate a major number dynamically
    major = register_chrdev(0, "cam_stream", &fops);
    if (major < 0) {
        printk(KERN_ERR "cam_stream init: Error registering a major number\n");
        return major;
    }
    printk(KERN_INFO "cam_stream init: Major Device Number - %d\n", major);

    // 2. Create a device class (appears in /sys/class/)
    cls = class_create("cam_class");
    if (IS_ERR(cls)) {
        unregister_chrdev(major, "cam_stream");
        printk(KERN_ALERT "cam_stream init: Failed to create class\n");
        return PTR_ERR(cls);
    }

    // 3. Create the Device Node (/dev/my_cam_stream)
    dev = device_create(cls, NULL, MKDEV(major, 0), NULL, "cam_stream");
    if (IS_ERR(dev)) {
        class_destroy(cls);
        unregister_chrdev(major, "cam_stream");
        printk(KERN_ALERT "cam_stream init: Failed to create Device Node\n");
        return PTR_ERR(dev);
    }

    // 4. Request GPIOs for LED
    red_led = gpio_to_desc(LED_RED_GPIO);
    green_led = gpio_to_desc(LED_GREEN_GPIO);

    if (!red_led || !green_led) {      
        printk(KERN_WARNING "cam_stream init: Failed to add to descriptor one or both GPIOs (RED:%d, GREEN:%d)\n", LED_RED_GPIO, LED_GREEN_GPIO);
        gpio_ready = false;

        // Clean up any that succeeded
        if (!red_led)
            gpio_free(LED_RED_GPIO);
        if (!green_led)
            gpio_free(LED_GREEN_GPIO);

    } else {      
        status_red = gpiod_direction_output(red_led, 1);
        status_green = gpiod_direction_output(green_led, 1);

        if (status_red || status_green) {
            printk(KERN_INFO "cam_stream init: Error setting GPIO %d or %d to output\n", LED_RED_GPIO, LED_GREEN_GPIO);
        }

        printk(KERN_INFO "cam_stream init: GPIO %d (RED) and %d (GREEN) initialized for LED\n", LED_RED_GPIO, LED_GREEN_GPIO);
        gpio_ready = true;

        // RED LED is the default at the start
        gpiod_set_value(red_led, 0);
        printk(KERN_INFO "cam_stream init: LED is RED\n");
    }

    printk(KERN_INFO "cam_stream init: Device created successfully\n\n");
    return 0;
}

static void __exit my_exit(void) 
{
    printk(KERN_INFO "cam_stream exit: Exiting\n");

    if (gpio_ready) {
        gpiod_set_value(red_led, 1);    // Reset the logic value of the GPIO to 0
        gpiod_set_value(green_led, 1);    

        gpiod_put(red_led);            // Release the GPIO resource
        gpiod_put(green_led); 

        printk(KERN_INFO "cam_stream exit: GPIO Resources released");
    } else {
        printk(KERN_INFO "cam_stream exit: No GPIO Resources to release");
    }

    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    unregister_chrdev(major, "cam_stream");

    printk(KERN_INFO "cam_stream exit: Unloaded successfully\n\n");
}

module_init(my_init);
module_exit(my_exit);


