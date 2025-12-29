#ifndef IMAGE_PROCESSOR_H
#define IMAGE_PROCESSOR_H

/**
* @file image_processor.h
* @brief 
*
* 
*/

#include <pthread.h>
#include <semaphore.h>

/* Forward declare structures */
struct camera_ctx;
struct stream_ctx;
struct yuyv_frame;
struct rgb_frame;
struct jpeg_frame;
typedef struct CircularBuffer CircularBuffer;

typedef struct pipeline_ctx {
    CircularBuffer *cb;
    pthread_mutex_t *mutex;
    sem_t *sem;
    struct camera_ctx *cctx;
    struct stream_ctx *sctx;
} pipeline_ctx;

int image_processor(struct yuyv_frame *yuyv, 
                    struct camera_ctx *cctx, 
                    struct stream_ctx *sctx,
                    struct pipeline_ctx *pipe);


#endif      /* IMAGE_PROCESSOR_H */