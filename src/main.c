/**
* @file main.c
* @brief Entry point for the Raspberry Pi MJPEG streaming.
*
* Longer description ...
*/

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>

#include "camera/camera.h"
#include "http/http_server.h"
#include "http/mjpeg_stream.h"
#include "cb/circular_buffer.h"
#include "image/image_encoder.h"
#include "image/image_processor.h"

#define THREAD_NUM      2
#define SERVER_PORT     8080

/** @brief Instantiate a circular buffer used for producer–consumer data exchange. */
CircularBuffer cb;

/**
* @brief Semaphore to indicate availability of data in the circular buffer.
* The consumer blocks on this semaphore when he buffer is empty, avoiding busy-waiting.
* The producer posts to this semaphore after inserting data. */
sem_t semData;

/**
* @brief Mutex protecting circular buffer access.
* Ensures mutual exclusion when modifying or reading circular buffer indices and entries. */
pthread_mutex_t mutexBuffer;

/** @brief Producer thread */
static void* producer(void* args) {
    pipeline_ctx *pipeline = args;
    if (capture_frames(pipeline->cctx, pipeline->sctx, pipeline) < 0) {
        perror("Producer breaking - Error in capturing frames");
    }

    return NULL;
}

/** @brief Consumer thread */
static void* consumer(void* args) {
    pipeline_ctx *pipeline = args;

    while(1) 
    {
        sem_wait(&semData);                     // BLOCK if no data
        if (send_frames(pipeline->cctx, pipeline->sctx, pipeline) < 0) {
            perror("Consumer breaking - Error in sending frames");
            break;
        }
    }

    return NULL;
}

int main(void) 
{
    circular_buffer_init(&cb);                  // Initialize circular buffer
    signal(SIGPIPE, SIG_IGN);                   // Ignore SIGPIPE to handle socket write error manually

    struct camera_ctx cctx = {0};               // Camera context (V4L2)
    struct stream_ctx sctx = {0};               // Streaming context (MJPEG)

    pthread_t producer_th;                      // Create storage for the thread
    pthread_t consumer_th;

    pthread_mutex_init(&mutexBuffer, NULL);     // Initialize the mutex
    sem_init(&semData, 0, 0);                   // Initialize the semphore

    pipeline_ctx pipeline = {
        .cb = &cb,
        .mutex = &mutexBuffer,
        .sem = &semData,
        .cctx = &cctx,
        .sctx = &sctx
    };

    // 1. Initialize the camera
    if (camera_init(&cctx) < 0) {
        fprintf(stderr, "Failed to initialize camera.\n");
        return -1;
    }

    // 2. Start Producer Thread ONCE
    if (pthread_create(&producer_th, NULL, &producer, &pipeline) != 0) {
        perror("Failed to create producer thread");
        return -1;
    }

    // 3. Start HTTP server (bind to port 8080)
    if (start_http_server(&sctx, SERVER_PORT) < 0) {
        fprintf(stderr, "main: Failed to start http server.\n");
        close_camera(&cctx);
        return -1;
    }
    printf("main: HTTP server listening on port %d\n", SERVER_PORT);

    // 4. Main server loop
    while(1) {
        printf("main: Waiting for a client...\n\n");

        // 4a. Accept a browser connection
        if (accept_client_connection(&sctx) < 0) {
            fprintf(stderr, "main: Failed to accept client.\n");
            continue;       // keep server alive
        }

        // 4b. Send HTTP multipart header
        if (send_mjpeg_http_header(&sctx) < 0) {
            fprintf(stderr, "main: Failed to send multipart header.\n");
            close(sctx.client_fd);
            continue;
        }

        // 4c. Start Consumer thread
        if (pthread_create(&consumer_th, NULL, &consumer, &pipeline) != 0) {
            perror("Failed to create consumer thread");
            close(sctx.client_fd);
            continue;
        }

        // Wait ONLY for consumer 
        pthread_join(consumer_th, NULL);

        printf("main: Client disconnected.\n\n");

        close(sctx.client_fd);
        sctx.client_fd = -1;
    }

    /* Close the camera and release resources */
    pthread_join(producer_th, NULL);                // Join producer thread
    sctx.server_fd = -1;
    sem_destroy(&semData);
    pthread_mutex_destroy(&mutexBuffer);
    return 0;
}