/**
* @file cam_stream.c
* @brief Kernel module providing LED control for camera streaming.
*
* This module exposes simple IOCTL commands that allow a user-space application
* to control two GPIO-driven LEDs. GREEN for "streaming ON" and RED for "streaming OFF".
* 
* The driver registers a character device and responds to:
*   - CAM_IOC_START - turn on GREEN LED (stream active)
*   - CAM_IOC_STOP - turn on RED LED (stream stopped)
*   - CAM_IOC_RESET - reset both LEDs to OFF
*/

#include <linux/fs.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/gpio/consumer.h>

#include "cam_stream_ioctl.h"

/** @brief Base GPIO offset used for LED mapping. */
#define GPIO_BASE                 571

/** @brief GPIO number for RED LED. */
#define LED_RED_GPIO        (GPIO_BASE + 21)

/** @brief GPIO number for GREEN LED. */
#define LED_GREEN_GPIO      (GPIO_BASE + 20)

/* -------------------------------------------------------------------------- */
/*                           Module Metadata                                  */
/* -------------------------------------------------------------------------- */
MODULE_LICENSE("GPL");              /**< Kernel module license. */
MODULE_AUTHOR("Hajj Smirky");       /**< Author. */
MODULE_DESCRIPTION("Character driver exposign IOCTL-based GPIO LED indicators for camera streaming state"); /**< Short description */

/* -------------------------------------------------------------------------- */
/*                        Module State Variables                              */
/* -------------------------------------------------------------------------- */

/** @brief Major number assigned to this character device. */
static int major;

/** @brief Device class used for /dev node creation. */
static struct class *cls;

/** @brief Device pointer for the registered /dev node. */
static struct device *dev;

/** @brief Flag for whether the LED GPIOs were successfully requested and configured. */
static bool gpio_ready = false;

/** @brief GPIO descriptor for RED LED. */
static struct gpio_desc *red_led;

/** @brief GPIO descriptor for GREEN LED. */
static struct gpio_desc *green_led;

/**
* @brief Open the cam_stream device.
*
* This function is invoked when a user space program opens the device node 
* '/dev/cam_stream'. It logs that the device has been opened.
*
* @param inode Pointer to the inode structure representing the device file.
* @param file Pointer to the file structure for the opened device.
*
* @return int 0 on success
*/
static int cam_stream_open(struct inode *inode, struct file *file) 
{
    printk(KERN_INFO "cam_stream open: /dev/cam_stream opened by user-space\n");
    return 0;
}

/**
* @brief Release the cam_stream device.
*
* This function is invoked when a user space process closes the device node
* '/dev/cam_stream'. It logs that the device has been released.
*
* @param inode Pointer to the inode structure representing the device file.
* @param file Pointer to the file structure for the device being closed.
*
* @return int 0 on success
*/
static int cam_stream_release(struct inode *inode, struct file *file) 
{
    printk(KERN_INFO "cam_stream release: /dev/cam_stream released by user-space\n");
    return 0;
}

/**
* @brief Handle custom IOCTL commands from user space for LED control.
* 
* This function is invoked when a user-space process calls 'ioctl()' on '/dev/cam_stream'.
* It interprets the command and toggles the corresponding GPIO LEDs to indicate camera
* streaming status.
*
* Supported commands:
*   - CAM_IOC_START : Turn GREEN LED on, RED LED off (camera streaming started)
*   - CAM_IOC_STOP  : Turn RED LED on, GREEN LED off (camera streaming stopped)
*   - CAM_IOC_RESET : Turn both RED and GREEN LEDs on (camera reset / YELLOW)
*
* If GPIOs are not initialized (`gpio_ready == false`), the LED actions are
* simulated via kernel log messages.
*
* @param file Pointer to the file structure representing the device file.
* @param cmd IOCTL command code (CAM_IOC_START, CAM_IOC_STOP, CAM_IOC_RESET).
* @param arg Unused argument
* 
* @return long
*           - 0 on success
*           - -EINVAL if an unknown IOCTL command is received
*/
static long cam_stream_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
        case CAM_IOC_START:
            /**
            * @note The LEDs are anode long-lead type:
            *           - Setting the GPIO to 1 turns the LED off
            *           - Setting the GPIO to 0 turns the LED on
            */
            printk(KERN_INFO "cam_stream ioctl: START command received\n");
            if (gpio_ready) {
                gpiod_set_value(red_led, 1);        // Turn off RED
                gpiod_set_value(green_led, 0);      // Turn on GREEN
                printk(KERN_INFO "cam_stream ioctl: LED is GREEN\n");
            } else {
                printk(KERN_INFO "cam_stream ioctl: LED is GREEN (Simulated - GPIO not ready)\n");
            }
            break;
        case CAM_IOC_STOP:
            printk(KERN_INFO "cam_stream ioctl: STOP command received\n");
            if (gpio_ready) {
                gpiod_set_value(green_led, 1);      // Turn off GREEN
                gpiod_set_value(red_led, 0);        // Turn on RED
                printk(KERN_INFO "cam_stream ioctl: LED is RED\n\n");
            } else {
                printk(KERN_INFO "cam_stream ioctl: LED is RED (Simulated - GPIO not ready)\n");
            }
            break;
        case CAM_IOC_RESET:
            printk(KERN_INFO "cam_stream ioctl: RESET command received\n");
            if (gpio_ready) {
                gpiod_set_value(green_led, 0);      // Turn on RED
                gpiod_set_value(red_led, 0);        // Turn on GREEN -> Turn on both RED + GREEN = YELLOW
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

/**
* @brief File operations structure for /dev/cam_stream
*
* This structure defines the set of operations that the kernel will invoke when a 
* user-space process interactes with the /dv/cam_stream device. It links system calls
* open(), release(), and ioctl() to the corresponding function in this driver.
*/
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = cam_stream_open,
    .release = cam_stream_release,
    .unlocked_ioctl = cam_stream_ioctl,
};

/**
* @brief Module initialization routine for the cam_stream driver.
*
* This function is executed when the module is loaded into the kernel. It performs all
* required setup to make the /dev/cam_stream device functional and ready for user-space 
* interaction.
*
* The initialization sequence includes:
*   1. Allocating a dynamic major device number.
*   2. Creating a device class entry under /sys/class/.
*   3. Creating the device node /dev/cam_stream.
*   4. Acquiring GPIO descriptors for the RED and GREEN LEDs.
*   5. Configuring both LEDS as output and setting the initial LED state.
*
* If any step fails, the function performs appropriate cleanup and returns an error code,
* preventing the driver from loading in a partially initialized state.
*
* @return 0 on successful initialization
*         -error code on failure
*/
static int __init my_init(void) 
{
    int status_red, status_green;

    printk(KERN_INFO "cam_stream init: Initializing...\n");

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

    // 3. Create the Device Node (/dev/cam_stream)
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

        // Release any GPIO descriptors that were successfully acquired
        if (red_led)
            gpiod_put(red_led);
        if (green_led)
            gpiod_put(green_led);

    } else {      

        // 5. Configure LEDs as output and set the initial LED state (OFF)
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

/**
* @brief Module cleanup routine for the cam_stream driver.
*
* This function is executed when the module is unloaded from the kernel. It reverses all the
* operations performed during initialization to ensure the system is left in a clean and
* consistent state.
*
* The cleanup steps include:
*   1. Restoring LED GPIOs to their default (inactive) state.
*   2. Releasing GPIO descriptors if they were successfully acquired.
*   3. Destroying the /dev/cam_stream device node.
*   4. Destroying the device class created under /sys/class.
*   5. Unregistering the dynamically allocated major number.
*/
static void __exit my_exit(void) 
{
    printk(KERN_INFO "cam_stream exit: Exiting...\n");

    if (gpio_ready) {
        // 1. Reset the GPIO to inactive state (1 = inactive, 0 = active)
        gpiod_set_value(red_led, 1);    
        gpiod_set_value(green_led, 1);    

        // 2. Release the GPIO descriptors
        gpiod_put(red_led);            
        gpiod_put(green_led); 

        printk(KERN_INFO "cam_stream exit: GPIO Resources released");
    } else {
        printk(KERN_INFO "cam_stream exit: No GPIO Resources to release");
    }

    // 3. Destroy device node
    device_destroy(cls, MKDEV(major, 0));

    // 4. Destroy device class
    class_destroy(cls);

    // 5. Unregister major number
    unregister_chrdev(major, "cam_stream");

    printk(KERN_INFO "cam_stream exit: Unloaded successfully\n\n");
}

/* Module initialization and cleanup callbacks */
module_init(my_init);
module_exit(my_exit);