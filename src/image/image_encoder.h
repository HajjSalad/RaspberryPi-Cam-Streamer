#ifndef IMAGE_ENCODER_H
#define IMAGE_ENCODER_H

/**
* @file image_encoder.h
* @brief 
*
* Declares data structures and functions used to convert raw YUYV422
* image buffers into JPEG-compressed frames using libjpeg.
*/

#include <stddef.h>

struct yuyv_frame {
    unsigned char *data;    /**< Pointer to raw YUYV422 frame data */
    unsigned int width;     /**< Frame width in pixels */
    unsigned int height;    /**< Frame height in pixels */
    unsigned long size;     /**< Size in bytes (width * height * 2) */
}

struct rgb_frame {
    unsigned char *data;    /**< Pointer to raw YUYV422 frame data */
    unsigned int width;     /**< Frame width in pixels */
    unsigned int height;    /**< Frame height in pixels */
    unsigned long size;     /**< Size in bytes (width * height * 3) */
}

struct jpeg_frame {
    unsigned char* data;    /**< Pointer to JPEG-compressed image data */
    unsigned long size;     /**< Size of the JPEG data in bytes */
};

/* Function Prototypes*/
void convert_yuyv_to_rgb(unsigned char* yuyv,
                        int width,
                        unsigned char* rgb);
void convert_rgb_to_jpeg(unsigned char* rgb,
                         int width,
                         int height,
                         struct jpeg_frame* frame);
int convert_yuyv_to_jpeg(unsigned char *yuyv_data,
                         int width,
                         int height,
                         struct jpeg_frame *frame);

#endif  /* JPEG_ENCODER_H */