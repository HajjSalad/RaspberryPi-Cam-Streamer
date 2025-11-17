#ifndef IOCTL_TEST_H
#define IOCTL_TEST_H

#include <linux/ioctl.h>

/**
* @file camera_client.h
* @brief Public API for V4L2 camera streaming and LED control
*
* 
*
*
*
*
* */

#define CAM_IOC_MAGIC 'k'
#define CAM_IOC_START _IO(CAM_IOC_MAGIC, 1)
#define CAM_IOC_STOP _IO(CAM_IOC_MAGIC, 2)
#define CAM_IOC_RESET _IO(CAM_IOC_MAGIC, 3)

struct camera_ctx;

/* --- Function prototypes --- */
int open_control_device(struct camera_ctx *ctx);
int led_stream_on(struct camera_ctx *ctx);
int led_stream_off(struct camera_ctx *ctx);
int configure_camera(struct camera_ctx *ctx);
int request_buffers(struct camera_ctx *ctx);
int map_buffers(struct camera_ctx *ctx);
int queue_buffers(struct camera_ctx *ctx);
int start_stream(struct camera_ctx *ctx);
int stop_stream(struct camera_ctx *ctx);
int capture_frames(struct camera_ctx *ctx);
void cleanup_buffers(struct camera_ctx *ctx);

#endif