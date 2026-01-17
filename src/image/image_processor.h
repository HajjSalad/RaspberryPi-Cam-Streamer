#ifndef IMAGE_PROCESSOR_H
#define IMAGE_PROCESSOR_H

/**
* @file image_processor.h
* @brief Image processing interface for the camera streaming pipeline.
*/

#include <pthread.h>
#include <semaphore.h>

// Forward declare structures
struct camera_ctx;
struct stream_ctx;
struct yuyv_frame;
struct rgb_frame;
struct jpeg_frame;
struct detector_ctx;
struct detection_result;
typedef struct CircularBuffer CircularBuffer;

/**
* @brief Pipeline context for the producer-consumer image pipeline.
*
* Holds references to the circular buffer, synchronization primitives, and 
* associated camera and streaming contexts used in the threads.
*/
typedef struct pipeline_ctx {
    CircularBuffer *cb;             /**< Pointer to the shared circular buffer */
    pthread_mutex_t *mutex;         /**< Mutex protecting access to the buffer */
    sem_t *sem;                     /**< Counting semaphore for frame availability */           
    struct camera_ctx *cctx;        /**< Pointer to the camera context */
    struct stream_ctx *sctx;        /**< Pointer to the streaming context */
} pipeline_ctx;

/** Function prototypes */
int image_processor(struct yuyv_frame *yuyv, 
                    struct camera_ctx *cctx, 
                    struct stream_ctx *sctx,
                    struct pipeline_ctx *pipe);

#endif      /* IMAGE_PROCESSOR_H */