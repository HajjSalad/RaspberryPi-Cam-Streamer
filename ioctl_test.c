#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "ioctl_test.h"

int main() {
    int fd = open("/dev/cam_stream", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    printf("Sending START command\n");
    ioctl(fd, CAM_IOC_START);

    sleep(2);

    printf("Sending STOP command\n");
    ioctl(fd, CAM_IOC_STOP);

    close(fd);
    return 0;
}