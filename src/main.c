/**
* @file main.c
* @brief Entry point for the Raspberry Pi MJPEG streaming server.
*
* This file initializes the context structures, sets up the HTTP server, accepts
* client connections, and starts the MJPEG streaming loop.
*
* The main responsibilities of this module include:
*   - System initialization and teardown
*   - HTTP server startup and client handling
*   - Coordinating camera capture and MJPEG streaming
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
#include "jpeg/jpeg_encoder.h"
#include "cb/circular_buffer.h"

#define THREAD_NUM      2
#define SERVER_PORT     8080

/**
* @brief Instantiate a circular buffer used for producer–consumer data exchange. 
*/
CircularBuffer cb;

/**
* @brief Semaphore to indicate availability of data in the circular buffer.
*
* The consumer blocks on this semaphore when he buffer is empty, avoiding busy-waiting.
* The producer posts to this semaphore after inserting data.
*/
sem_t semData;

/**
* @brief Mutex protecting circular buffer access.
*
* Ensures mutual exclusion when modifying or reading circular buffer indices and 
* entries in the multithreaded context.
*/
pthread_mutex_t mutexBuffer;

/* Function Prototypes */
static void* producer(void* args);
static void* consumer(void* args);

int main(void) 
{
    circular_buffer_init(&cb);                  // Initialize circular buffer
    signal(SIGPIPE, SIG_IGN);                   // Ignore SIGPIPE to handle socket write error manually

    struct camera_ctx cctx = {0};               // Camera context (V4L2)
    struct stream_ctx sctx = {0};               // Streaming context (MJPEG)
    struct jpeg_frame frame = {0};              // JPEG-compressed frame buffer

    pthread_t th[THREAD_NUM];                   // Create storage for thread IDs

    pthread_mutex_init(&mutexBuffer, NULL);     // Initialize the mutex
    sem_init(&semData, 0, 0);                   // Initialize the semphore


    // 1. Initialize the camera
    if (initialize_camera(&cctx) < 0) {
        fprintf(stderr, "Failed to initialize camera.\n");
        return -1;
    }

    // 2. Start HTTP server (bind to port 8080)
    if (start_http_server(&sctx, SERVER_PORT) < 0) {
        fprintf(stderr, "main: Failed to start http server.\n");
        close_camera(&cctx);
        return -1;
    }
    printf("main: HTTP server listening on port %d\n", SERVER_PORT);

    // 3. Main server loop
    while(1) {
        printf("main: Waiting for a client...\n\n");

        // 3a. Accept a browser connection
        if (accept_client_connection(&sctx) < 0) {
            fprintf(stderr, "main: Failed to accept client.\n");
            continue;       // keep server alive
        }

        // 3b. Send HTTP multipart header
        if (send_mjpeg_http_header(&sctx) < 0) {
            fprintf(stderr, "main: Failed to send multipart header.\n");
            close(sctx.client_fd);
            continue;
        }

        // Alternate to create and start producer-consumer threads
        for (int i=0; i<THREAD_NUM; i++) {
            if (i % 2 == 0) {
                if (pthread_create(&th[i], NULL, &producer, NULL) != 0) {
                    perror("Failed to create producer thread");
                }
            } else {
                if (pthread_create(&th[i], NULL, &consumer, NULL) != 0) {
                    perror("Failed to create consumer thread");
                }
            }
        } 

        // Join the threads
        for (int i=0; i<THREAD_NUM; i++) {
            if (pthread_join(th[i], NULL) != 0) {
                perror("Failed to join the threads");
            }
        }

        printf("main: Client disconnected.\n\n");

        close(sctx.client_fd);
        sctx.client_fd = -1;
    }

    /* Close the camera and release resources */
    close_camera(&cctx);
    close(sctx.server_fd);
    sctx.server_fd = -1;
    sem_destroy(&semData);
    pthread_mutex_destroy(&mutexBuffer);
    return 0;
}

static void* producer(void* args) {
    while(1) {

        capture_frames(frame, cctx, sctx);

        // Add to the buffer
        pthread_mutex_lock(&mutexBuffer);
        cb_write(&cb, frame);                           // overwrite allowed
        pthread_mutex_unlock(&mutexBuffer);

        sem_post(&semData);                             // Signal availability
    }
}

static void* consumer(void* args) {
    struct jpeg_frame *output;

    while(1) {
        sem_wait(&semData);                     // BLOCK if no data

        pthread_mutex_lock(&mutexBuffer);
        cb_read(&cb, &output);
        pthread_mutex_unlock(&mutexBuffer);

        //  Send JPEG frame to client
        int ret = send_mjpeg_frame(output, sctx);
        if (ret < 0) {
            // Check errno for more details if needed
            fprintf(stderr, "Client disconnected or send error (ret=%d)\n", ret);
            break;  // exit streaming loop
        }

        // Free allocated JPEG memory
        free(frame->data);
        frame->data = NULL;
        frame->size = 0;
    }
}