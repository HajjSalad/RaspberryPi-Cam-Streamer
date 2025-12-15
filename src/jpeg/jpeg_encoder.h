#ifndef JPEG_ENCODER_H
#define JPEG_ENCODER_H

/**
* @file jpeg_encoder.h
* @brief JPEG encoder interface for raw YUYV422 camera frames.
*
* Declares data structures and functions used to convert raw YUYV422
* image buffers into JPEG-compressed frames using libjpeg.
*/

#include <stddef.h>

/**
* @brief Represents a single JPEG-compressed frame.
*
* This structure stores the pointer and size of the JPEG image produced
* from the raw YUYV data. The memory is reused each frame to avoid allocations.
*/
struct jpeg_frame {
    unsigned char* data;            /**< Pointer to JPEG-compressed image data */
    unsigned long size;             /**< Size of the JPEG data in bytes */
};

/**
* @brief Encode a YUYV422 frame into a JPEG image.
*
* Converts a raw YUYV422 image buffer into a JPEG-compressed image
* and stores the result in the provided jpeg_frame structure.
*
* @param[in]  yuyv_data   Pointer to raw YUYV422 image buffer
* @param[in]  width       Image width in pixels
* @param[in]  height      Image height in pixels
* @param[out] frame       Output JPEG frame (data and size)
*
* @return 0 on success, non-zero on failure
*/
int convert_yuyv_to_jpeg(unsigned char *yuyv_data,
                         int width,
                         int height,
                         struct jpeg_frame *frame);

#endif  /* JPEG_ENCODER_H */