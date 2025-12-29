#ifndef MJPEG_STREAM_H
#define MJPEG_STREAM_H

// Forward declare the context structures
struct camera_ctx;
struct stream_ctx;
struct jpeg_frame;
struct pipeline_ctx;

/**
* @brief Streaming context for MJPEG server.
*
* Holds the server socket and client socket used during streaming.
*/
struct stream_ctx {
    int server_fd;                 /**< Listening socket for the HTTP/MJPEG server */        
    int client_fd;                 /**< Connected client socket */
};

// Function Prototypes
int send_frames(struct camera_ctx *cctx, struct stream_ctx *sctx, struct pipeline_ctx *pipe);
int send_mjpeg_frame(struct jpeg_frame *frame, struct stream_ctx *sctx);

#endif  // MJPEG_STREAM_H