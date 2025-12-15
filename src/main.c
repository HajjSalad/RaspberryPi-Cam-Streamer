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

#include "camera/camera.h"
#include "http/http_server.h"
#include "http/mjpeg_stream.h"
#include "jpeg/jpeg_encoder.h"

#define SERVER_PORT     8080

int main(void) 
{
    struct camera_ctx cctx = {0};       // Camera context (V4L2)
    struct stream_ctx sctx = {0};       // Streaming context (MJPEG)
    struct jpeg_frame frame = {0};      // JPEG-compressed frame buffer

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
        printf("main: Waiting for a client...\n");

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

        // 3c. Stream frames until disconnect
        if (handle_http_client(&frame, &cctx, &sctx) < 0) {
            fprintf(stderr, "main: Streaming error.\n");
        }

        printf("main: Client disconnected.\n");

        close(sctx.client_fd);
        sctx.client_fd = -1;
    }

    /* Close the camera and release resources */
    close_camera(&cctx);
    close(sctx.server_fd);
    sctx.server_fd = -1;
    return 0;
}

