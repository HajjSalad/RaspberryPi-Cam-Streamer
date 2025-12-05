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
#include <jpeglib.h>
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
    void *start;   /**< Pointer to the start of the mapped buffer in user space*/
    size_t length;  /**< Size of the buffer in bytes */
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
    struct v4l2_buffer buf;         /**< Temporary buffer struct for operations */+

    struct buffer *buffers;         /**< Pointer to an array of mapped buffers */
    unsigned int n_buffers;         /**< Number of mapped buffers */
};

/**
* @brief Represents a single JPEG-compressed frame.
*
* This structure stores the pointer and size of the JPEG image produced
* from the raw YUYV data. The memory is reused each frame to avoid allocations.
*/
struct jpeg_frame {
    unsigned char* data;            /**< Pointer to JPEG-compressed image data */
    unsigned long size;             /**< Size of the JPEG data in bytes */
};

/**
* @brief Streaming context for MJPEG output.
*
* Stores per-client streaming state, including the file descriptors and the buffer
* that holds the JPEG-encoded frame ready to be sent over the network.
* 
* This separation keeps MJPEG transmission state modular.
*/
struct stream_ctx {
    int server_fd;                 /**< HTTP/MJPEG server socket */        
    int client_fd;                 /**< Connected client socket */

    struct jpeg_frame frame;       /**< JPEG frame buffer for the current output frame */
}

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
int capture_frames(struct camera_ctx *ctx, struct stream_ctx *sctx) {
    
    printf("camera_client: Capturing for %d seconds...\n", STREAM_DURATION);

    unsigned char jpeg_buffer[500000];
    unsigned long jpeg_size;
    
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

        // Convert YUYV to JPEG
        convert_yuyv_to_jpeg(
            ctx->buffers[ctx->buf.index].start,
            ctx->fmt.fmt.pix.width,
            ctx->fmt.fmt.pix.height,
            &sctx->frame
        );

        // Send MPJEG frame
        send_mjpeg_frame(&sctx->frame, sctx->client_fd);

        // Free JPEG after sending
        free(frame->data);
        frame->data = NULL;
        frame->size = 0;
        
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
* @brief Convert a raw YUYV422 frame into a compressed JPEG image.
* 
* This function takes a single raw camera frame in YUYV422 format (YUV 4:2:2) 
* and compresses it into JPEG format using libjpeg. The output JPEG data is 
* written into a caller-provided memory buffer.
*
* Conversion pipeline:
*   - Camera produces YUYV422 (YUV packed format)
*   - Function converts YUYV -> RGB24 (per scanline)
*   - RGB24 is fed into the libjpeg compressor
*
* Processing Steps:
*   1. Initialize libjpeg compression object and error handler
*   2. Configure the compressor to write output into an in-memory buffer
*   3. Set image parameters (width, height, components, color space)
*   4. Apply default JPEG compression settings:
*       - Sets standard Huffman tables
*       - Prepares internal structures for scanline compression
*       - Ensures reasonable defaults for quality, quantization, and optimization
*   5. Convert YUYV422 -> RGB24 one scanline at a time
*   6. Pass RGB scanlines to the JPEG compressor
*   7. Finalize JPEG output (write end markers)
*   8. Release libjpeg resources
*
* @param[in]  yuyv_data    Pointer to raw YUYV422 image buffer.
* @param[in]  width        Width of the input image in pixels.
* @param[in]  height       Height of the input image in pixels.
* @param[out] frame        Pointer to jpeg_frame structure that will receive:
*                           - frame->buffer : allocated JPEG image buffer
*                           - frame->size : size of JPEG data in bytes
*
* @return 0 on success, non-zero on failure
*
* @note Caller is responsible for freeing frame->buffer. libjpeg allocates it.
*/
int convert_yuyv_to_jpeg(unsigned char* yuyv_data,
                         int width,
                         int height,
                         struct jpeg_frame *frame)
{
    struct jpeg_compress_struct cinfo;      // main JPEG compression object
    struct jpeg_error_mgr jerr;             // Error manager struct used by libjpeg

    // Link cinfo compression object to libjpeg internal error-handling system
    cinfo.err = jpeg_std_error(&jerr);

    // 1. Initializes the compressor, allocate internal memory and prep cinfo for compression
    jpeg_create_compress(&cinfo);        

    // 2. Set output destination to memory buffer
    jpeg_mem_dest(&cinfo, &frame->data, &frame->size);

    // 3. Image parameters
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;         // # of color components in input image (RGB = 3 channels)
    cinfo.in_color_space = JCS_RGB;     // color space of input image (Input is RGB)

    // 4. Default config
    jpeg_set_defaults(&cinfo);          
    jpeg_set_quality(&cinfo, 80, TRUE);         // 80 = good balance

    jpeg_start_compression(&cinfo, TRUE);       // TRUE = write full Q-tables and Huffman tables

    // Temporary buffer to store one RGB scanline
    unsigned char *row = malloc(width * 3);     // Each scanline = width pixels * 3 bytes (R,G, B)

    // 5. Convert YUYV -> RGB and feed scanlines into libjpeg
    while (cinfo.next_scanline < cinfo.image_height) {

        // Pointer to the start of the YUYV rows
        unsigned char *yuyv = yuyv_data + (cinfo.next_scanline * width * 2);

        // Convert pairs of pixels
        for (int x = 0; x < width; x += 2) {

            int y0 = yuyv[0];
            int u = yuyv[1];
            int y1 = yuyv[2];
            int v = yuyv[3];

            // Convert YUV to RGB (BT.601 standard)
            #define CLIP(x) ( (x)<0 ? 0 : ( (x)>255 ? 255 : (x) ) )

            int c = y0 - 16;
            int d = u - 128;
            int e = v - 128;

            // Pixel 0
            int r0 = CLIP((298*c + 409*e + 128) >> 8);
            int g0 = CLIP((298*c - 100*d - 208*e + 128) >> 8);
            int b0 = CLIP((298*c + 516*d + 128) >> 8);

            // Pixel 1
            c = y1 - 16;
            int r1 = CLIP((298*c + 409*e + 128) >> 8);
            int g1 = CLIP((298*c - 100*d - 208*e + 128) >> 8);
            int b1 = CLIP((298*c + 516*d + 128) >> 8);

            // Store RGB results into row buffer correctly
            row[(x * 3) + 0] = r0;
            row[(x * 3) + 1] = g0;
            row[(x * 3) + 2] = b0;

            row[(x * 3) + 3] = r1;
            row[(x * 3) + 4] = g1;
            row[(x * 3) + 5] = b1;

            // Move to the next YUYV block (4 bytes)
            yuyv += 4;
        }

        // Feed one scanline to libjpeg
        JSAMPROW row_pointer[1] = { row };
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    // Finish compression - Writes end-of-image marker, flushes buffers, finalizes memory
    jpeg_finish_compress(&cinfo);

    // Cleanup libjpeg resources
    jpeg_destroy_compress(&cinfo);

    free(row);

    return 0;
}

/**
* @brief Send a single JPEG image as an MJPEG frame over an HTTP multipart stream.
* 
* This function writes the following structure to the client socket:
*
*   --frame\r\n                         (multipart boundary marker)
*   Content-Type: image/jpeg\r\n
*   Content-Length: <jpeg_size>\r\n
*   \r\n                                (separator btwn headers and binary data)
*   <JPEG BINARY DATA>\r\n              (/r/n -> end-of-frame terminator)
*   \r\n
*
* This matches the MJPEG-over-HTTP format used by web browsers and video players
* when receiving multipart/x-mixed-replace streams.
*
* @param sctx   Pointer to stream context structure that contains:
*                   - client_fd : active client socket
*                   - frame.data : pointer to JPEG buffer
*                   - frame.size : size of JPEG buffer
*
* @return 0 on success, negative value on error.
*/
int send_mjpeg_frame(struct jpeg_frame *frame, int client_fd) 
{
    if (!frame || !frame->data || frame->size == 0) {
        return -1;
    }

    // Construct MJPEG frame header
    char header[256];
    int header_len = snprintf(header, sizeof(header),
        "--frame\r\n"                       // multipart boundary marker
        "Content-Type: image/jpeg\r\n"
        "Content-Length: %lu\r\n"
        "\r\n",                             // separator btwn headers and binary data
        frame->size
    );

    // 1. Send multipart header
    if (write(client_fd, header, header_len) != header_len) {
        return -2;      // header write error
    }

    // 2. Send JPEG binary payload
    if (write(client_fd, frame->data, frame->size) != (size_t)frame->size) {
        return -3;      // payload write error
    }

    // 3. Send end-of-frame terminator
    const char *end = "\r\n";
    if (write(client_fd, end, 2) != 2) {
        return -4;      // Footer write error
    }

    return 0;           // success
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