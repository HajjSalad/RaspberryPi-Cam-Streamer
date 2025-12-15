#ifndef MJPEG_STREAM_H
#define MJPEG_STREAM_H

// Forward declare the context structures
struct jpeg_frame;

/**
* @brief Streaming context for MJPEG server.
*
* Holds the server socket and client socket used during streaming.
*/
struct stream_ctx {
    int server_fd;                 /**< Listening socket for the HTTP/MJPEG server */        
    int client_fd;                 /**< Connected client socket */
};

/**
* @brief Send JPEG image as MJPEG frame over an HTTP multipart stream.
*
* @param frame  Pointer to the JPEG image produced from YUYV data.
* @param sctx   Pointer to structure context that contains client_fd.
*
* @return 0 on success, negative value on error.e
*/
int send_mjpeg_frame(struct jpeg_frame *frame, struct stream_ctx *sctx);

#endif  // MJPEG_STREAM_H