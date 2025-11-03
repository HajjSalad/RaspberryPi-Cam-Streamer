#ifndef IOCTL_TEST_H
#define IOCTL_TEST_H

#include <linux/ioctl.h>

#define CAM_IOC_MAGIC 'k'
#define CAM_IOC_START _IO(CAM_IOC_MAGIC, 1)
#define CAM_IOC_STOP _IO(CAM_IOC_MAGIC, 2)
#define CAM_IOC_RESET _IO(CAM_IOC_MAGIC, 3)

#endif