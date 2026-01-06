#include "detector.h"
#include <stdio.h>

int main()
{
    struct detection_ctx dctx = {0};

    if (detector_init(&dctx) < 0) {
        printf("test: detector_init failed\n");
        return -1;
    }

    printf("test: detector initialized successfully\n");
    return 0;
}
