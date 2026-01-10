/**
* @file image_processor.c
* @brief Image processing stage of the camera streaming pipeline.
*
* This modules implements the core image processing pipeline used by the procuder thread.
*/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#include "image_encoder.h"
#include "camera/camera.h"
#include "image_processor.h"
#include "http/mjpeg_stream.h"
#include "cb/circular_buffer.h"

/**
* @brief Process a captured camera frame and enqueue it for streaming.
*
* Performs the following pipeline stages:
*   1. Convert a raw YUYV camera frame to RGB
*   2. Encode the RGB frame into JPEG format
*   3. Push the encoded frame into a shared circular buffer
*   4. Signal frame availability to the consumer thread
*
* @note This function represents the producer stage of the producer-consumer streaming pipeline.
*
* @param yuyv   Pointer to the captured YUYV frame from the camera
* @param cctx   Pointer to the camera context structure that holds all session state
* @param sctx   Pointer to the stream structure context that holds stream sessions
* @param pipe   Pointer to the pipeline context containing thread and synchronization primitives
*
* @return 0 on success, -1 on failure
*/
int image_processor(struct yuyv_frame *yuyv, 
                    struct camera_ctx *cctx, 
                    struct stream_ctx *sctx,
                    struct pipeline_ctx *pipe)
{ 
    struct rgb_frame rgb = {0};                            // Stack-allocated RGB frame 
    struct jpeg_frame *jpeg = calloc(1, sizeof(*jpeg));    // Heap-allocated JPEG frame
    if (!jpeg) return -1;

    // 1. YUYV -> RGB
    if (convert_yuyv_to_rgb(yuyv, &rgb) != 0) {
        perror("Error converting YUYV to RGB");
        goto cleanup;
    }

    // 2. RGB -> JPEG
    if (convert_rgb_to_jpeg(&rgb, jpeg) != 0) {
        perror("Error converting RGB to JPEG");
        goto cleanup;
    }

    // 3. Push JPEG into circular buffer
    pthread_mutex_lock(pipe->mutex);
    cb_write(pipe->cb, jpeg);                        // overwrite allowed
    pthread_mutex_unlock(pipe->mutex);

    sem_post(pipe->sem);                             // Signal frame availability

    free(rgb.data);
    return 0;

cleanup:
    free(rgb.data);
    free(jpeg);
    return -1;
}