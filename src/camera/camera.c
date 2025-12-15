/**
* @file camera.c
* @brief Implementation of the V4L2 camera initialization and teardown routines.
*
* This module handles all low-level operations required to prepare a V4L2 camera
* device for streaming, including:
*   - Opening /dev/video0
*   - Requesting streaming buffers
*   - Memory-mapping kernel buffers to userspace
*   - Queueing buffers for capture
*   - Starting and stopping the video stream
*   - Releasing all allocated resources on shutdown
* 
* Only two public functions are exposed through camera.h:
*   - initialize_camera()
*   - close_camera()
*
* All other help functions are kept private to ensure proper encapsulation and
* prevent external modules from depending on internal implementation details.
*/

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "camera.h"
#include "http/mjpeg_stream.h"
#include "jpeg/jpeg_encoder.h"
#include "cam_stream_ioctl.h"

/** @brief Internal helper functions.  */
static int open_control_device(struct camera_ctx *cctx);
static int configure_camera(struct camera_ctx *cctx);
static int request_mmap_buffers(struct camera_ctx *cctx);
static int map_buffers(struct camera_ctx *cctx);
static int queue_buffers(struct camera_ctx *cctx);
static int start_stream(struct camera_ctx *cctx);
static int stop_stream(struct camera_ctx *cctx);
static int led_stream_on(struct camera_ctx *cctx);
static int led_stream_off(struct camera_ctx *cctx);
static void cleanup_buffers(struct camera_ctx *cctx);

/**
* @brief Initialize and prepare the camera device for streaming
*
* This function sets up the camera context and performs the required sequence
* of V4L2 initialization steps.
*
* On failure, the function automatically cleans up all resources allocated 
* up to the failure point by calling close_camera().
*
* @param cctx Pointer to the camera context structure that will be initialized.
*             
* @return 0 on success
*         Negative value on failure
*
* @note This is part of the public camera API and is exposed via camera.h.
*/
int initialize_camera(struct camera_ctx *cctx)
{
    memset(cctx, 0, sizeof(*cctx));
    cctx->cam_fd = -1;
    cctx->dev_fd = -1;


    if (open_control_device(cctx) < 0) goto error;
    if (configure_camera(cctx) < 0) goto error;
    if (request_mmap_buffers(cctx) < 0) goto error;
    if (map_buffers(cctx) < 0) goto error;
    if (queue_buffers(cctx) < 0) goto error;
    if (start_stream(cctx) < 0) goto error;

    return 0;

error:
    close_camera(cctx);
    return -1;
}

/**
* @brief Stop the camera stream and release all associated resources.
* 
* This function performs full shutdown of the camera context:
*   - Stops the V4L2 video stream if it is running
*   - Unmaps and frees all MMAP buffers
*   - Closes the camera and control device file descriptors
*
* @param cctx Pointer to a camera context structure whose resources 
*             should be cleaned up. 
*
* @return void
*
* @note This is part of the public camera API and is exposed via camera.h
*/

void close_camera(struct camera_ctx *cctx)
{
    if (!cctx) return;

    stop_stream(cctx);         // If stream started
    cleanup_buffers(cctx);     // If mmap buffers allocated

    if (cctx->cam_fd >= 0) {
        close(cctx->cam_fd);
        cctx->cam_fd = -1;
    }

    if (cctx->dev_fd >= 0) {
        close(cctx->dev_fd);
        cctx->dev_fd = -1;
    }

    printf("camera: Devices closed\n");
}

/**
* @brief Open the control device (/dev/cam_stream).
*
* This function opens the custom kernel driver used for LED signaling
* and camera control. It attempts to open @ref DEVICE_PATH with read/write 
* permission and stores the resulting file descriptor in the global 
* variable @ref ctx->dev_fd.
*
* @param cctx Pointer to the camera context structure that holds all session state.
*
* @return int
*           - 0 on success
*           - -1 on failure (error message printed using perror)
*/
static int open_control_device(struct camera_ctx *cctx) {
    cctx->dev_fd = open(DEVICE_PATH, O_RDWR);
    if (cctx->dev_fd < 0) {
        perror("camera: Failed to open /dev/cam_stream");
        return -1;
    }
    printf("camera: Device /dev/cam_stream opened successfully\n");
    return 0;
}

/**
* @brief Initializes and configures the camera device (dev/video0)
* 
* This function opens the camera device with read/write access and applies
* a predefined video capture format based on the Logitech C270 HD webcam 
* specs obtained from 'c4l2-ctl -all'.
*
* @param cctx Pointer to the camera context structure that holds all session state.
* @return int:
*           - 0 on success
*           - -1 on failure (error message printed using perror)
*/
static int configure_camera(struct camera_ctx *cctx) {
    // --- Open camera device (/dev/video0) ---
    cctx->cam_fd = open(CAMERA_PATH, O_RDWR);
    if (cctx->cam_fd < 0) {
        perror("camera: Failed to open /dev/video0");
        return -errno;
    }
    printf("camera: Device /dev/video0 opened successfully\n");

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

    memset(&(cctx->fmt), 0, sizeof(cctx->fmt));
    cctx->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cctx->fmt.fmt.pix.width = 640;
    cctx->fmt.fmt.pix.height = 480;
    cctx->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    cctx->fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(cctx->cam_fd, VIDIOC_S_FMT, &cctx->fmt) < 0) {
        perror("camera: Failed to set format");
        close(cctx->cam_fd);
        cctx->cam_fd = -1;
        return -errno;
    }

    printf("camera: Camera configuration successful\n");
    return 0;
}

/**
* @brief Requests memory-mapped buffers from the video device.
*
* Initializes the v4l2_requestbuffers structure and requests a 
* fixed number of buffers (4) for memory-mapped I/O.
*
* @param cctx Pointer to the camera context structure that holds all session state.
* @return int
*           - 0 on success
*           - -errno on failure
* */
static int request_mmap_buffers(struct camera_ctx *cctx) {
    memset(&cctx->req, 0, sizeof(cctx->req));
    cctx->req.count = 4;
    cctx->req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cctx->req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(cctx->cam_fd, VIDIOC_REQBUFS, &cctx->req) < 0) {
        perror("camera: Failed to request buffers");
        return -errno;
    }

    printf("camera: Buffer request successful\n");
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
static int map_buffers(struct camera_ctx *cctx) 
{ 
    // Allocate array describing each buffer in user-space
    cctx->buffers = calloc(cctx->req.count, sizeof(*(cctx->buffers)));
    if (!(cctx->buffers)) {
        perror("camera: Failed to allocate buffer array");
        return -errno;
    }

    cctx->n_buffers = cctx->req.count;

    // Map each kernel buffer into user space
    for (unsigned int i = 0; i < cctx->n_buffers; i++) {

        // Prepare QUERYBUF structure
        memset(&cctx->buf, 0, sizeof(cctx->buf));
        cctx->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cctx->buf.memory = V4L2_MEMORY_MMAP;
        cctx->buf.index = i;

        // Query buffer metadata from the driver
        if (ioctl(cctx->cam_fd, VIDIOC_QUERYBUF, &cctx->buf) < 0) {
            perror("camera: Failed querying the buffer");
            return -errno;
        }

        // Store metadata amd map the kernel buffer into user space
        cctx->buffers[i].length = cctx->buf.length;
        cctx->buffers[i].start = mmap(NULL, 
                                    cctx->buf.length,
                                    PROT_READ | PROT_WRITE, 
                                    MAP_SHARED,
                                    cctx->cam_fd, 
                                    cctx->buf.m.offset);

        if(cctx->buffers[i].start == MAP_FAILED) {
            perror("camera: Failed mapping the buffer");

            // Only unmap the buffers that were successfully mapped
            cleanup_buffers(cctx);
            cctx->buffers = NULL;
            return -errno;
        }
    }

    printf("camera: Mapping successful\n");
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
* @param cctx Pointer to the camera context structure that holds all session state.
* @return int
*           - 0 on success
*           - -errno on failure
*/
static int queue_buffers(struct camera_ctx *cctx) {
    for (unsigned int i = 0; i < cctx->n_buffers; i++) {
        memset(&cctx->buf, 0, sizeof(cctx->buf));

        // Set buffer type, memory mode, and index
        cctx->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cctx->buf.memory = V4L2_MEMORY_MMAP;
        cctx->buf.index = i;

        // Queue the buffer to the driver
        if(ioctl(cctx->cam_fd, VIDIOC_QBUF, &cctx->buf) < 0) {
            perror("camera: Failed to queue buffer");
            return -errno;
        }
    }

    printf("camera: Buffer queue successful\n");
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
* @param cctx Pointer to the camera context structure that holds all session state.
* @return int
*           - 0 on success
*           - -errno on failure
*/
static int start_stream(struct camera_ctx *cctx) 
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // Request the driver to start streaming
    if (ioctl(cctx->cam_fd, VIDIOC_STREAMON, &type) < 0) {
        perror("camera: Failed to start the stream");
        return -errno;
    }
    
    printf("camera: Stream started...\n");
    
    // Notify LED driver that streaming has begun
    led_stream_on(cctx);

    return 0;
}

/**
* @brief Captures and outputs video frames.
*
* This function runs the main capture loop. It repeteadly:
*   1. Dequeues a filled buffer using VIDIOC_DQBUF
*   2. Converts the YUYV data to JPEG image
*   3. Sends the JPEG frame to the client
*   4. Re-queues the buffer with VIDIOC_QBUF for reuse
*
* @param cctx   Pointer to the camera context structure that holds camera sessions.
* @param sctx   Pointer to the stream structure context.
*
* @note STREAMON must have been called before entering this loop.
* @note Buffers must already be requested, mapped, and queued.
*
* @return 0 on success, negative value on error.
*/
int capture_frames(struct jpeg_frame *frame, struct camera_ctx *cctx, struct stream_ctx *sctx) 
{
    for (;;) {
        // Prepare the buffer struct
        memset(&cctx->buf, 0, sizeof(cctx->buf));
        cctx->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cctx->buf.memory =V4L2_MEMORY_MMAP;

        // 1. Dequeue a frame buffer
        if (ioctl(cctx->cam_fd, VIDIOC_DQBUF, &cctx->buf) < 0) {
            perror("camera: Failed to dequeue buffer");
            break;
        }

        // 2. Convert YUYV to JPEG
        if (convert_yuyv_to_jpeg(
                cctx->buffers[cctx->buf.index].start,
                cctx->fmt.fmt.pix.width,
                cctx->fmt.fmt.pix.height,
                frame
            ) != 0) {
            perror("camera: Error converting YUYV to JPEG");
        }

        // 3. Send PJEG frame to client
        if (send_mjpeg_frame(frame, sctx) < 0) {
            fprintf(stderr, "Client disconnected or send error.\n");
            break;
        }

        // 4. Free allocated JPEG memory
        free(frame->data);
        frame->data = NULL;
        frame->size = 0;
        
        // 5. Requeue the buffer to be filled again
        if (ioctl(cctx->cam_fd, VIDIOC_QBUF, &cctx->buf) < 0){
            perror("camera: Failed to requeue buffer");
            break;
        }
    } 

    printf("Capture stopped.\n");
    return 0;
}

/**
* @brief Stops the video capture stream.
* 
* This function disables the video stream after capture is complete.
* It also triggers the LED control via led_stream_off() to indicate that 
* streaming has stopped.
*
* @param cctx Pointer to the camera context structure that holds all session state.
* @return int
*           - 0 on success
*           - -errno on failure
*/
static int stop_stream(struct camera_ctx *cctx) 
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // Request the driver to stop streaming
    if (ioctl(cctx->cam_fd, VIDIOC_STREAMOFF, &type) < 0) {
        perror("camera: Failed to stop the stream");
        return -errno;
    }
    
    printf("camera: Stream stopped.\n");
    
    // Notify LED driver that streaming has ended
    led_stream_off(cctx);

    return 0;
}

/**
* @brief Turns the LED GREEN to indicate that streaming has started
* 
* Sends CAM_IOC_START to the LED control device to switch the LED green
*
* @param cctx Pointer to the camera context structure that holds all session state.
* @return int
*           - 0 on success
*           - -errno on failure
*/
int led_stream_on(struct camera_ctx *cctx) {
    if (ioctl(cctx->dev_fd, CAM_IOC_START) < 0) {
        perror("camera: Failed to send LED GREEN command");
        return -errno;
    }

    printf("camera: Turn LED GREEN command sent\n");
    return 0;
}

/**
* @brief Turns the LED RED to indicate that streaming has stopped
* 
* Sends CAM_IOC_STOP to the LED control device to switch the LED red
*
* @param cctx Pointer to the camera context structure that holds all session state.
* @return int
*           - 0 on success
*           - -errno on failure
*/
int led_stream_off(struct camera_ctx *cctx) {
    if (ioctl(cctx->dev_fd, CAM_IOC_STOP) < 0) {
        perror("camera: Failed to send LED RED command");
        return -errno;
    }

    printf("camera: Turn LED RED command sent\n");
    return 0;
}

/**
* @brief Unmaps all V4L2 buffers and frees buffer array.
*
* @param cctx Pointer to the camera context structure that holds all session state.
*/
void cleanup_buffers(struct camera_ctx *cctx)
{
    if (!cctx->buffers) return;

    for (unsigned int i = 0; i < cctx->n_buffers; i++) {
        if (cctx->buffers[i].start && cctx->buffers[i].start != MAP_FAILED) {
            munmap(cctx->buffers[i].start, cctx->buffers[i].length);
        }
    }

    free(cctx->buffers);
    cctx->buffers = NULL;
    cctx->n_buffers = 0;
}
