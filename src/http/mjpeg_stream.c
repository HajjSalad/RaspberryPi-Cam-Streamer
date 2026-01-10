/**
* @file mjpeg_stream.c
* @brief MJPEG frame streaming over HTTP.
*
* This module implements the MJPEG-over-HTTP streaming layer, responsible
* for transmitting JPEG-encoded frames to connected clients using the
* multipart/x-mixed-replace format.
*/

#include <stdio.h>     
#include <unistd.h>    
#include <stddef.h>    
#include <stdlib.h> 

#include "mjpeg_stream.h"
#include "camera/camera.h"
#include "http/http_server.h"
#include "http/mjpeg_stream.h"
#include "cb/circular_buffer.h"
#include "image/image_encoder.h"
#include "image/image_processor.h"

/** Function Prototypes */
static int send_mjpeg_frame(struct jpeg_frame *frame, struct stream_ctx *sctx);

/**
* @brief Consume JPEG frames from the pipeline and stream them to the client
*
* Performs the following pipeline stages:
*   1. Retrieve a JPEG frame from the shared circular buffer
*   2. Send the JPEG frame to the connected client as MJPEG frame
*   3. Release associated memory after transmission
*
* @note This function represents the consumer stage of the producer-consumer streaming pipeline.
*
* @param cctx   Pointer to the camera context structure that holds all session state
* @param sctx   Pointer to the stream structure context that holds stream sessions
* @param pipe   Pointer to the pipeline context containing thread and synchronization primitives
* 
* @return 0 on success, negative value on error
*/
int send_frames(struct camera_ctx *cctx, 
                struct stream_ctx *sctx, 
                struct pipeline_ctx *pipe) 
{
    struct jpeg_frame *jpeg = NULL;

    // Retrieve th JPEG frame from the circular buffer
    pthread_mutex_lock(pipe->mutex);
    cb_read(pipe->cb, &jpeg);
    pthread_mutex_unlock(pipe->mutex);

    if (!jpeg) { return 0; }

    //  Send JPEG frame to client
    int ret = send_mjpeg_frame(jpeg, sctx);
    if (ret < 0) {
        fprintf(stderr, "Client disconnected or send error (ret=%d)\n", ret);
        free(jpeg->data);
        free(jpeg);
        return -1;
    }

    // Free allocated JPEG memory
    free(jpeg->data);
    free(jpeg);

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
* @param frame  Pointer to the JPEG image produced from YUYV data.
* @param sctx   Pointer to structure context that contains client_fd.
*
* @return 0 on success, negative value on error.
*/
static int send_mjpeg_frame(struct jpeg_frame *frame, struct stream_ctx *sctx) 
{
    if (!frame || !frame->data || frame->size == 0) {
        printf("Frames empty/not available");
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
    if (write(sctx->client_fd, header, header_len) != header_len) {
        return -2;      // header write error
    }

    // 2. Send JPEG binary payload
    if (write(sctx->client_fd, frame->data, frame->size) != (size_t)frame->size) {
        return -3;      // payload write error
    }

    // 3. Send end-of-frame terminator
    const char *end = "\r\n";
    if (write(sctx->client_fd, end, 2) != 2) {
        return -4;      // Footer write error
    }

    return 0;           // success
}