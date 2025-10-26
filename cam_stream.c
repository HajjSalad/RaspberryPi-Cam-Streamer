#include <linux/fs.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/gpio/consumer.h>
#include "camera_client.h"

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
// Purpose:
//      - Log that /dev/cam_stream has been opened
//      - Internally open the camera device (/dev/video0) using filp_open()
//      - Store the camera's file pointer in /dev/cam_stream's file structure so that
//        subsequent read(), write(), or ioctl() operations can access it later
static int cam_stream_open(struct inode *inode, struct file *file) 
{
    struct file *camera_filp;

    printk(KERN_INFO "cam_stream open: /dev/cam_stream opened by user-space\n");

    // Attempt to open the camera device handled by V4L2
    camera_filp = filp_open("/dev/video0", O_RDWR, 0);
    if (IS_ERR(camera_filp)) {
        printk(KERN_ERR "cam_stream open: Failed to open /dev/video0\n");
        return PTR_ERR(camera_filp);
    }

    printk(KERN_INFO "cam_stream open: Opened /dev/video0 successfully\n");

    // Save the camera file pointer so other file operations can use it
    file->private_data = camera_filp;

    return 0;
}

// read() -> Invoked when a user-space process calls read(fd, buffer, size)
// Purpose:
//      - Allocate temporary kernel buffer to hold the data read from /dev/video0
//      - Read bytes from the camera device into the kernel buffer
//      - Copy the data from the kernel space to user space
static ssize_t cam_stream_read(struct file *file, char *buf, size_t len, loff_t *offset) 
{
    struct file *camera_filp;
    char *kbuf;
    ssize_t ret;

    // Get camera file from private_data
    camera_filp = file->private_data;
    if (!camera_filp)
        return -ENODEV;

    // Temporary kernel buffer to hold the data from the camera device
    // Allocate temporary kernel buffer
    kbuf = kmalloc(len, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    // Read from actual /dev/video0 into kernel buffer
    ret = kernel_read(camera_filp, kbuf, len, &camera_filp->f_pos);
    if (ret > 0) {
        // Copy the data to user space
        if (copy_to_user(buf, kbuf, ret)) {
            kfree(kbuf);       
            return -EFAULT;
        }
    }

    kfree(kbuf);
    return ret;         // Return number of bytes read, a negative error code
}

// release() -> Invoked when user space process calls close(fd)
// Purpose: 
//      - Close the camera device (/dev/video0) opened in open()
//      - Clear private data pointer
static int cam_stream_release(struct inode *inode, struct file *file) 
{
    struct file *camera_filp;

    // Close the camera device (/dev/video0)
    camera_filp = file->private_data;
    if (camera_filp) {
        filp_close(camera_filp, NULL);
        printk(KERN_INFO "cam_stream release: /dev/video0 closed successfully\n");
    } else {
        printk(KERN_INFO "cam_stream release: No /dev/video0 to close\n");
    }

    // Clear the private_data pointer
    file->private_data = NULL;

    printk(KERN_INFO "cam_stream release: /dev/cam_stream released by user-space\n");
    return 0;
}

// ioctl() -> Invoked when user-space process issues an ioctl() on /dev/cam_stream
// Purpose:
//      - Handle custom camera control commands (CAM_IOC_START, CAM_IOC_STOP, CAM_IOC_RESET)
//      - Toggle GPIO LEDs to indicate camera status (RED, GREEN, YELLOW)
//      - For unrecognized ioctl commands (e.g., native V4L2 ioctls like VIDIOC_STREAMON),
//        forward the command to the actual camera device (/dev/video0) using the saved
//        camera file pointer in file->private_data
static long cam_stream_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct file *camera_filp;

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
                printk(KERN_INFO "cam_stream ioctl: LED is RED\n");
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
            // Forward unknown ioctls (V4L2 native IOCTLs eg VIDIOC_STREAMON) to /dev/video0
            camera_filp = file->private_data;   
            if (camera_filp && camera_filp->f_op && camera_filp->f_op->unlocked_ioctl) 
                return camera_filp->f_op->unlocked_ioctl(camera_filp, cmd, arg);
            
            return -ENOTTY;     // Inappropriate ioctl for device
    }
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = cam_stream_open,
    .read = cam_stream_read,
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

    printk(KERN_INFO "cam_stream init: Device created successfully\n");
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


