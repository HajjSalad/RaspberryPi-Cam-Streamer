#ifndef CAMERA_H
#define CAMERA_H

/**
* @file camera.h
* @brief Public API for V4L2 camera initialization, and teardown.
*/

#include <stddef.h>
#include <linux/videodev2.h>

/** @brief Path to the LED/camera control deivce. */
#define DEVICE_PATH         "/dev/cam_stream"

/** @brief Path to the V4L2 camera device. */
#define CAMERA_PATH         "/dev/video0"

// Forward declare the context structures
struct stream_ctx;
struct yuyv_frame;
struct pipeline_ctx;

/**
* @brief Describes a single memory-mapped video buffer.
* 
* This structure holds the starting address and length of a buffer
* that is mapped into user-space from the kernel by the V4L2 driver.
* Each buffer corresponds to one frame that the camera can write to.
*/
struct buffer {
    void *start;   /**< Pointer to the start of the mapped buffer in user space*/
    size_t length;  /**< Size of the buffer in bytes */
};

/**
* @brief Aggregates all state required for a V4L2 camera streaming session.
*
* This context structure stores file descriptors, V4L2 configuration, buffer
* metadata, and pointers to memory-mapped frame buffers. 
* All camera operations take a pointer to this context instead of relying on global
* variables, improving modularity and maintainability.
*/
struct camera_ctx {
    int dev_fd;                     /**< File descriptor for LED/control device */
    int cam_fd;                     /**< File descriptor for camera device */

    struct v4l2_format fmt;         /**< Video format configuration */
    struct v4l2_requestbuffers req; /**< Requested buffers information */
    struct v4l2_buffer buf;         /**< Temporary buffer struct for operations */

    struct buffer *buffers;         /**< Pointer to an array of mapped buffers */
    unsigned int n_buffers;         /**< Number of mapped buffers */
};

/* Function Prototypes */
int camera_init(struct camera_ctx *cctx);
void close_camera(struct camera_ctx *cctx);
int capture_frames(struct camera_ctx *cctx, struct stream_ctx *sctx, struct pipeline_ctx *pipeline);

#endif /* CAMERA_H */