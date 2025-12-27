/**
* @file http_server.c
* @brief HTTP server handler module.
*
* Implements basic HTTP request handling and response generation.
* Provides initialization, routing, and utilities for sending responses.
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "http_server.h"
#include "mjpeg_stream.h"
#include "camera/camera.h"

/**
* @brief Initializes and starts a simple HTTP server for MJPEG streaming.
*
* This function creates a TCP socket, enables address reuse, binds it to 
* the port, and begins listening for incoming client connections.
* The resulting listening socket is stored in the stream context.
*
* @param sctx   Pointer to the stream context where the server socket will be stored.
* @param port   TCP port on which the server will listen
*
* @return 0 on success, -1 on failure
*/
int start_http_server(struct stream_ctx *sctx, unsigned short port)
{
    int opt = 1;
    struct sockaddr_in addr;        // Internet IPv4 socket address

    /* 1. Create an IPv4 TCP socket.
    *  socket() arguements:
    *   1. domain: AF_INET (protocol family) -> IPv4 Internet protocol
    *   2. socket type: SOCK_STREAM -> connection-oriented stream (TCP)
    *   3. protocol = 0; -> exact protocol inside selected domain + type.
    */
    sctx->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sctx->server_fd < 0) {
        perror("http_server: socket");
        return -1;
    }

    /* 2. Allow immediate reuse of the port after program restarts */
    if (setsockopt(sctx->server_fd, SOL_SOCKET, SO_REUSEADDR, 
                    &opt, sizeof(opt)) < 0) {
        perror("http_server: setsockopt");
        close(sctx->server_fd);
        return -1;
    }

    /* 3. Prepare server address structure */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family       = AF_INET;            // IPv4
    addr.sin_addr.s_addr  = htonl(INADDR_ANY);  // Listen on all interfaces
    addr.sin_port         = htons(port);        // Convert port to network byte order

    /* Bind the socket to the chosen address + port */
    if (bind(sctx->server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("http_server: bind");
        close(sctx->server_fd);
        return -1;
    }

    /* 4. Mark socket as listening (ready to accept clients) */
    if (listen(sctx->server_fd, 4) < 0) {
        perror("http_server: listen");
        close(sctx->server_fd);
        return -1;
    }

    return 0;
}

/**
* @brief Accepts an incoming HTTP client connection.
*
* This function blocks until a client connects to the listening socket.
* On success, the newly accepted client is stored in 'sctx->client_fd'.
* It also prints the remote client's IP and port.
*
* @param sctx Pointer to stream context containing listening server socket.
*
* @return The client file descriptor on success, -1 on failure.
*/
int accept_client_connection(struct stream_ctx *sctx) 
{
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

    // Block until a client connects
    sctx->client_fd = accept(sctx->server_fd, (struct sockaddr*)&client_addr, &addrlen);
    if (sctx->client_fd < 0) {
        perror("http_server: accept");
        return -1;
    }

    // Convert and display client IP
    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &client_addr.sin_addr, buf, sizeof(buf))) {
        printf("http_server: Accepted connection from %s:%d\n", buf, ntohs(client_addr.sin_port));
    } else {
        printf("http_server: Accepted connection from <unknown>\n");
    }
    
    return sctx->client_fd;
}

/**
* @brief Send a minimal HTTP header for MJPEG streaming.
*
* Writes the required HTTP response headers to the socket to initialize 
* an MJPEG multipart stream over HTTP.
*
* @param sctx Pointer to stream context containing listening server socket.
*
* @return 0 on success, -1 on failure
*/
int send_mjpeg_http_header(struct stream_ctx *sctx)
{
    // Send HTTP header
    const char *header =
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
        "\r\n";
        
    ssize_t written = write(sctx->client_fd, header, strlen(header));
    if (written != (ssize_t)strlen(header)) {
        perror("write header");
        return -1;
    }

    return 0;
}

/**
* @brief Handle a single HTTP client connection for MJPEG streaming.
*
* This function enters the frame capture/stream loop and continues sending 
* JPEG frames until the client disconnects or an error occurs.
*
* @param cctx Camera capture context.
* @param sctx Stream context containing the client socket.
* @return 0 on normal disconnect, -1 on error.
*/
int handle_http_client(struct jpeg_frame *frame, struct camera_ctx *cctx, struct stream_ctx *sctx) 
{
    // Now start sending frames
    printf("http_server: Sending JPEG frames to client now...\n");
    return capture_frames(frame, cctx, sctx);
}