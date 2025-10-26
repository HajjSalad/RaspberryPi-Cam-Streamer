#ifndef IOCTL_TEST_H
#define IOCTL_TEST_H

#include <linux/ioctl.h>

#define MY_CAM_IOC_MAGIC 'k'
#define MY_CAM_IOC_START _IO(MY_CAM_IOC_MAGIC, 1)
#define MY_CAM_IOC_STOP _IO(MY_CAM_IOC_MAGIC, 2)
#define MY_CAM_IOC_RESET _IO(MY_CAM_IOC_MAGIC, 3)

#endif