#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

/**
* @file http_server.h
* @brief Simple HTTP server API for streaming MJPEG data.
*
* Provides functions for starting a minimal HTTP server, accepting incoming
* client connections, sending The MJPEG response header, and streaming frames
* to the client.
*/

// Forward declare the context structures
struct camera_ctx;
struct stream_ctx;
struct jpeg_frame;

/* Function prototypes */
int start_http_server(struct stream_ctx *sctx, unsigned short port);
int accept_client_connection(struct stream_ctx *sctx);
int send_mjpeg_http_header(struct stream_ctx *sctx);
int handle_http_client(struct jpeg_frame *frame, struct camera_ctx *cctx, struct stream_ctx *sctx);

#endif  // HTTP_SERVER_H