#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include "camera_client.h"

#define DEVICE_PATH         "/dev/cam_stream"
#define CAMERA_PATH         "/dev/video0"
#define OUTPUT_FILE         "camera_stream.raw"
#define STREAM_DURATION     10

int main() {
    int dev_fd, cam_fd;
    FILE *out;
    ssize_t bytes_read;
    time_t start_time, current_time;
    
    // Open control device 
    dev_fd = open(DEVICE_PATH, O_RDWR);
    if (dev_fd < 0) {
        perror("camera_client: Failed to open /dev/cam_stream\n");
        return 1;
    }
    printf("camera_client: Device /dev/cam_stream opened successfully\n");

    // Send START command
    if (ioctl(dev_fd, CAM_IOC_START) < 0) {
        perror("camera_client: Failed to send START command");
        close(dev_fd);
        return 1;
    }
    printf("camera_client: START command sent. Streaming starts soon\n");

    // open camera device
    cam_fd = open(CAMERA_PATH, O_RDWR);
    if (cam_fd < 0) {
        perror("camera_client: Failed to open /dev/video0\n");
        ioctl(dev_fd, CAM_IOC_STOP);
        close(dev_fd);
        return 1;
    }
    printf("camera_client: Device /dev/video0 opened successfully\n");

    // C270 HD WEBCAM 
    // Format Video Capture: (command: `v4l2-ctl --all`)
    //     Width/Height      : 640/480
    //     Pixel Format      : 'YUYV' (YUYV 4:2:2)
    //     Field             : None
    //     Bytes per Line    : 1280
    //     Size Image        : 614400
    //     Colorspace        : sRGB
    //     Transfer Function : Rec. 709
    //     YCbCr/HSV Encoding: ITU-R 601
    //     Quantization      : Default (maps to Limited Range)
    //     Flags             : 

    // Set camera format -> This ensures the video is captured with these certain parameters
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(cam_fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("camera_client: Failed to set format");
        goto stop;
    }

    // Request buffers -> Request buffers from kernel space for the captures frames
    // which will then be mapped into the user space
    memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(cam_fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("camera_client: Failed to request buffers");
        goto stop;
    }

    

    // // Open output file to store raw captured frames
    // out = fopen(OUTPUT_FILE, "wb");
    // if (!out) {
    //     perror("camera_client: Failed to open the output file");
    //     ioctl(fd, CAM_IOC_STOP);
    //     close(fd);
    //     return 1;
    // } else {
    //     printf("camera_client: Output file opened successfully\n");
    // }

    // Read frames for STREAM_DURATION seconds
    time(&start_time);
    do {
        printf("Streaming...\n");
        sleep(1);                   // Wait for 1 second
        time(&current_time);
    } while (difftime(current_time, start_time) < STREAM_DURATION);

    // Send STOP command
    printf("camera_client: Sending STOP command\n");
    if (ioctl(dev_fd, CAM_IOC_STOP) < 0) {
        perror("camera_client: Failed to send STOP command");
    }

    // Close file and device
    fclose(out);
    close(dev_fd);
    close(cam_fd);
    printf("camera_client: Device closed and file saved as '%s'\n");
    
    return 0;
}