#ifndef CAM_STREAM_IOCTL_H
#define CAM_STREAM_IOCTL_H

#include <linux/ioctl.h>

/**
* @file cam_stream_ioctl.h
* @brief Shared IOCTL definitions for cam_stream kernel module and userspace.
*
* Provides the IOCTL command codes used by both the kernel module and the userspace
* application.
*/

/** @brief Magic number for IOCTL commands */
#define CAM_IOC_MAGIC 'k'

/** @brief IOCTL command to start LED (GREEN) */
#define CAM_IOC_START _IO(CAM_IOC_MAGIC, 1)

/** @brief IOCTL command to stop LED (RED) */
#define CAM_IOC_STOP _IO(CAM_IOC_MAGIC, 2)

/** @brief IOCTL command to reset the device */
#define CAM_IOC_RESET _IO(CAM_IOC_MAGIC, 3)

#endif /* CAM_STREAM_IOCTL_H */