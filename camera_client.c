/**
* @file camera_client.c
* @brief User-space V4L2 camera streaming client with LED signaling
*
* This module implements a complete user-space pipeline for streaming video
* from a V4L2 camera device (`/dev/video0`) while cordinating LED signaling
* through a custom kernel control driver (`/dev/cam_stream`).
*
* The main tasks performed by this module include:
*    - Opening and configuring the camera
*    - Requesting, memory-mapping and, queuing V4L2 buffers
*    - Starting the video stream
*    - Capturing and outputting frames
*    - Cleaning up all allocated resources
*
* All per-session state is stored in a `struct camera_ctx`, allowing the API
* to operate without global variables.
*/

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "camera_client.h"
#include <linux/videodev2.h>

/** @brief Path to the LED/camera control deivce. */
#define DEVICE_PATH         "/dev/cam_stream"

/** @brief Path to the V4L2 camera device. */
#define CAMERA_PATH         "/dev/video0"

/** @brief Duration for video capture, in seconds. */
#define STREAM_DURATION     10

/**
* @brief Describes a single memory-mapped video buffer.
* 
* This structure holds the starting address and length of a buffer
* that is mapped into user-space from the kernel by the V4L2 driver.
* Each buffer corresponds to one frame that the camera can write to.
*/
struct buffer {
    void   *start;   /**< Pointer to the start of the mapped buffer in user space*/
    size_t  length;  /**< Size of the buffer in bytes */
};

/**
* @brief Aggregates all state required for a V4L2 camera streaming session.
*
* This context structure stores file descriptors, V4L2 configuration, buffer
* metadata, and pointers to memory-mapped frame buffers. All camera operations 
* take a pointer to this context instead of relying on global variables,
* improving modularity and maintainability.
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

/**
* @brief Open the control device (/dev/cam_stream).
*
* This function opens the custon kernel driver used for LED signaling
* and camera control. It attempts to open @ref DEVICE_PATH with read/write 
* permission and stores the resulting file descriptor in the global 
* variable @ref ctx->dev_fd.
*
* @param ctx Pointer to the camera context structure that holds all session state.
*
* @return int
*           - 0 on success
*           - -1 on failure (error message printed using perror)
*/
int open_control_device(struct camera_ctx *ctx) {
    ctx->dev_fd = open(DEVICE_PATH, O_RDWR);
    if (ctx->dev_fd < 0) {
        perror("camera_client: Failed to open /dev/cam_stream");
        return -1;
    }
    printf("camera_client: Device /dev/cam_stream opened successfully\n");
    return 0;
}

/**
* @brief Turns the LED GREEN to indicate that streaming has started
* 
* Sends CAM_IOC_START to the LED control device to switch the LED green
*
* @param ctx Pointer to the camera context structure that holds all session state.
* @return int
*           - 0 on success
*           - -errno on failure
*/
int led_stream_on(struct camera_ctx *ctx) {
    if (ioctl(ctx->dev_fd, CAM_IOC_START) < 0) {
        perror("camera_client: Failed to send LED GREEN command");
        return -errno;
    }

    printf("camera_client: Turn LED GREEN command sent\n");
    return 0;
}

/**
* @brief Turns the LED RED to indicate that streaming has stopped
* 
* Sends CAM_IOC_STOP to the LED control device to switch the LED red
*
* @param ctx Pointer to the camera context structure that holds all session state.
* @return int
*           - 0 on success
*           - -errno on failure
*/
int led_stream_off(struct camera_ctx *ctx) {
    if (ioctl(ctx->dev_fd, CAM_IOC_STOP) < 0) {
        perror("camera_client: Failed to send LED RED command");
        return -errno;
    }

    printf("camera_client: Turn LED RED command sent\n");
    return 0;
}

/**
* @brief Initializes and configures the camera device (dev/video0)
* 
* This function opens the camera device with read/write access and applies
* a predefined video capture format based on the Logitech C270 HD webcam 
* specs obtained from 'c4l2-ctl -all'.
*
* @param ctx Pointer to the camera context structure that holds all session state.
* @return int:
*           - 0 on success
*           - -1 on failure (error message printed using perror)
*/
int configure_camera(struct camera_ctx *ctx) {
    // --- Open camera device (/dev/video0) ---
    ctx->cam_fd = open(CAMERA_PATH, O_RDWR);
    if (ctx->cam_fd < 0) {
        perror("camera_client: Failed to open /dev/video0");
        return -errno;
    }
    printf("camera_client: Device /dev/video0 opened successfully\n");

   /*  
    * Based on Logitech C270 HD Webcam capabilities (from `v4l2-ctl --all`):
    *   Width/Height      : 640 / 480
    *   Pixel Format      : YUYV (YUYV 4:2:2)
    *   Field             : None (progressive)
    *   Colorspace        : sRGB
    *   Transfer Function : Rec. 709
    *   Encoding          : ITU-R 601
    *
    * We explicitly set width, height, pixel format, and field.  
    * The driver fills the remaining fields if needed.
    */

    memset(&(ctx->fmt), 0, sizeof(ctx->fmt));
    ctx->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ctx->fmt.fmt.pix.width = 640;
    ctx->fmt.fmt.pix.height = 480;
    ctx->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    ctx->fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(ctx->cam_fd, VIDIOC_S_FMT, &ctx->fmt) < 0) {
        perror("camera_client: Failed to set format");
        close(ctx->cam_fd);
        ctx->cam_fd = -1;
        return -errno;
    }

    printf("camera_client: Camera configuration successful\n");
    return 0;
}

/**
* @brief Requests memory-mapped buffers from the video device.
*
* Initializes the v4l2_requestbuffers structure and requests a 
* fixed number of buffers (4) for memory-mapped I/O.
*
* @param ctx Pointer to the camera context structure that holds all session state.
* @return int
*           - 0 on success
*           - -errno on failure
* */
int request_buffers(struct camera_ctx *ctx) {
    memset(&ctx->req, 0, sizeof(ctx->req));
    ctx->req.count = 4;
    ctx->req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ctx->req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(ctx->cam_fd, VIDIOC_REQBUFS, &ctx->req) < 0) {
        perror("camera_client: Failed to request buffers");
        return -errno;
    }

    printf("camera_client: Buffer request successful\n");
    return 0;
}

/**
* @brief Maps kernel-allocated V4L2 buffers into user space
*
* After buffers are requested with VIDIOC_REQBUFS (see @ref request_buffers()),
* this function retrieves metadata for each buffer using VIDIOC_QUERYBUF and 
* maps them into user space via mmap(). The mapped buffers allow the application 
* to directly access video frames written by the camera driver without copies.
*
* @param ctx Pointer to the camera context structure that holds all session state.
* @return int
*           - 0 on success
*           - -errno on failure
*
* @note On success, the caller is responsible for unmapping the buffers using 
*       munmap() and freeing the `buffers` array.
*/
int map_buffers(struct camera_ctx *ctx) 
{ 
    // Allocate array describing each buffer in user-space
    ctx->buffers = calloc(ctx->req.count, sizeof(*(ctx->buffers)));
    if (!(ctx->buffers)) {
        perror("camera_client: Failed to allocate buffer array");
        return -errno;
    }

    ctx->n_buffers = ctx->req.count;

    // Map each kernel buffer into user space
    for (unsigned int i = 0; i < ctx->n_buffers; i++) {

        // Prepare QUERYBUF structure
        memset(&ctx->buf, 0, sizeof(ctx->buf));
        ctx->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ctx->buf.memory = V4L2_MEMORY_MMAP;
        ctx->buf.index = i;

        // Query buffer metadata from the driver
        if (ioctl(ctx->cam_fd, VIDIOC_QUERYBUF, &ctx->buf) < 0) {
            perror("camera_client: Failed querying the buffer");
            return -errno;
        }

        // Store metadata amd map the kernel buffer into user space
        ctx->buffers[i].length = ctx->buf.length;
        ctx->buffers[i].start = mmap(NULL, 
                                    ctx->buf.length,
                                    PROT_READ | PROT_WRITE, 
                                    MAP_SHARED,
                                    ctx->cam_fd, 
                                    ctx->buf.m.offset);

        if(ctx->buffers[i].start == MAP_FAILED) {
            perror("camera_client: Failed mapping the buffer");

            // Only unmap the buffers that were successfully mapped
            cleanup_buffers(ctx);
            ctx->buffers = NULL;
            return -errno;
        }
    }

    printf("camera_client: Mapping successful\n");
    return 0;
}

/**
* @brief Queues memory-mapped buffers to the video device
*
* After buffers are mapped into user space using @ref map_buffers(),
* they must be queued to the kernel driver before streaming. This allows
* the driver to know which buffers are available for the camera to write 
* captured frames into.
*
* @param ctx Pointer to the camera context structure that holds all session state.
* @return int
*           - 0 on success
*           - -errno on failure
*/
int queue_buffers(struct camera_ctx *ctx) {
    for (unsigned int i = 0; i < ctx->n_buffers; i++) {
        memset(&ctx->buf, 0, sizeof(ctx->buf));

        // Set buffer type, memory mode, and index
        ctx->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ctx->buf.memory = V4L2_MEMORY_MMAP;
        ctx->buf.index = i;

        // Queue the buffer to the driver
        if(ioctl(ctx->cam_fd, VIDIOC_QBUF, &ctx->buf) < 0) {
            perror("camera_client: Failed to queue buffer");
            return -errno;
        }
    }

    printf("camera_client: Buffer queue successful\n");
    return 0;
}

/**
* @brief Starts the video capture stream.
* 
* This function enables the video stream after all the buffers have been
* requested, mapped, and queued.
* 
* On success, this function also triggers the LED control via led_stream_on()
* to indicate that streaming is active.
*
* @param ctx Pointer to the camera context structure that holds all session state.
* @return int
*           - 0 on success
*           - -errno on failure
*/
int start_stream(struct camera_ctx *ctx) 
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // Request the driver to start streaming
    if (ioctl(ctx->cam_fd, VIDIOC_STREAMON, &type) < 0) {
        perror("camera_client: Failed to start the stream");
        return -errno;
    }
    
    printf("camera_client: Stream started...\n");
    
    // Notify LED driver that streaming has begun
    led_stream_on(ctx);

    return 0;
}

/**
* @brief Stops the video capture stream.
* 
* This function disables the video stream after capture is complete.
* It also triggers the LED control via led_stream_off() to indicate that 
* streaming has stopped.
*
* @param ctx Pointer to the camera context structure that holds all session state.
* @return int
*           - 0 on success
*           - -errno on failure
*/
int stop_stream(struct camera_ctx *ctx) 
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // Request the driver to stop streaming
    if (ioctl(ctx->cam_fd, VIDIOC_STREAMOFF, &type) < 0) {
        perror("camera_client: Failed to stop the stream");
        return -errno;
    }
    
    printf("camera_client: Stream stopped.\n");
    
    // Notify LED driver that streaming has ended
    led_stream_off(ctx);

    return 0;
}

/**
* @brief Captures and outputs video frames.
*
* This function runs the main capture loop. It repeteadly:
*   1. Dequeues a filled buffer using VIDIOC_DQBUF
*   2. Writes the frame data to output stream (ex. stdout)
*   3. Re-queues the buffer with VIDIOC_QBUF for reuse
*
* @param ctx Pointer to the camera context structure that holds all session state.
*
* @note STREAMON must have been called before entering this loop.
* @note Buffers must already be requested, mapped, and queued.
*/
int capture_frames(struct camera_ctx *ctx) {
    
    printf("camera_client: Capturing for %d seconds...\n", STREAM_DURATION);
    
    time_t start_time, current_time;
    time(&start_time);

    do {
        memset(&ctx->buf, 0, sizeof(ctx->buf));
        ctx->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ctx->buf.memory =V4L2_MEMORY_MMAP;

        // Dequeue a frame buffer
        if (ioctl(ctx->cam_fd, VIDIOC_DQBUF, &ctx->buf) < 0) {
            perror("camera_client: Failed to dequeue buffer");
            break;
        }

        // Write frame data to stdout (or any target file)
        fwrite(ctx->buffers[ctx->buf.index].start, ctx->buf.bytesused, 1, stdout);
        
        // Requeue the buffer to be filled again
        if (ioctl(ctx->cam_fd, VIDIOC_QBUF, &ctx->buf) < 0){
            perror("camera_client: Failed to requeue buffer");
            break;
        }

        time(&current_time);
    } while (difftime(current_time, start_time) < STREAM_DURATION);

    printf("Capture complete. Stopping stream...\n");
    return 0;
}

/**
* @brief Unmaps all V4L2 buffers and frees buffer array.
*
* @param ctx Pointer to the camera context structure that holds all session state.
*/
void cleanup_buffers(struct camera_ctx *ctx)
{
    if (!ctx->buffers) return;

    for (unsigned int i = 0; i < ctx->n_buffers; i++) {
        if (ctx->buffers[i].start && ctx->buffers[i].start != MAP_FAILED) {
            munmap(ctx->buffers[i].start, ctx->buffers[i].length);
        }
    }

    free(ctx->buffers);
    ctx->buffers = NULL;
    ctx->n_buffers = 0;
}

/**
 * @brief Entry point for the camera streaming client.
 * 
 * This function coordinates the entire video capture session:
 *  1. Opens the LED/control device.
 *  2. Configures the camera device.
 *  3. Requests and maps memory-mapped buffers.
 *  4. Queues buffers to the kernel driver.
 *  5. Starts the video stream and turns the LED green.
 *  6. Captures video frames for a predefined duration.
 *  7. Stops the video stream and turns the LED red.
 *  8. Cleans up all allocated resources (buffers and device file descriptors).
 * 
 * The function ensures that resources are properly released even if
 * an intermediate step fails.
 * 
 * @return int
 *         - 0 if all operations succeeded
 *         - 1 if any operation failed
 */
int main(void) {
    struct camera_ctx ctx = {0};
    int ret = 0;

    if (open_control_device(&ctx) < 0) { ret = 1; goto cleanup; }
    if (configure_camera(&ctx) < 0) { ret = 1; goto cleanup; }
    if (request_buffers(&ctx) < 0) { ret = 1; goto cleanup; }
    if (map_buffers(&ctx) < 0) { ret = 1; goto cleanup; }
    if (queue_buffers(&ctx) < 0) { ret = 1; goto cleanup; }
    if (start_stream(&ctx) < 0) { ret = 1; goto cleanup; }
    if (capture_frames(&ctx) < 0) { ret = 1; goto cleanup; }
    if (stop_stream(&ctx) < 0) { ret = 1; goto cleanup; }

cleanup:
    cleanup_buffers(&ctx);

    if (ctx.cam_fd >= 0) close(ctx.cam_fd);
    if (ctx.dev_fd >= 0) close(ctx.dev_fd);

    printf("camera_client: Devices closed\n");
    return ret;
}