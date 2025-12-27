/**
* @file image_processor.c
* @brief
*
*
*
*
*/

int image_processor(struct yuyv_frame *yuyv, 
                    struct camera_ctx *cctx, 
                    struct stream_ctx *sctx,
                    pipeline_ctx *pipe)
{ 
    struct rgb_frame rgb = {0};
    struct jpeg_frame *jpeg = malloc(sizeof(*jpeg));
    if (!jpeg) return -1;

    // 1. YUYV -> RGB
    if (convert_yuyv_to_rgb(yuyv, &rgb) != 0) {
        perror("Error converting YUYV to RGB");
        goto cleanup;
    }

    if (motion_detected) {
        run_object_detection(&rgb, &result);
    }

    // 2. RGB -> JPEG
    if (convert_rgb_to_jpeg(&rgb, jpeg) != 0) {
        perror("Error converting RGB to JPEG");
        goto cleanup;
    }

    // 3. Push JPEG into circular buffer
    pthread_mutex_lock(pipe->mutex);
    cb_write(pipe->cb, frame);                           // overwrite allowed
    pthread_mutex_unlock(pipe->mutex);

    sem_post(pipe->sem);                             // Signal availability

    free(rgb.data);
    return 0;

cleanup:
    free(rgb.data);
    free(jpeg);
    return -1;
}