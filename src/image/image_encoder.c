/**
* @file image_encoder.c
* @brief Image format conversion and JPEG compression implementation
*
* Provides:
*   1. Conversion from YUYV422 to RGB24
*   2. JPEG compression of RGB frames using libjpeg
*
* These routines are designed to prepare frames for MJPEG HTTP transmission.
*/

#include <stdio.h>   
#include <stdlib.h> 
#include <setjmp.h>
#include <jpeglib.h>   

#include "image_encoder.h"

/** @brief Ensure value stays within 8-bit pixel range: [0, 255]  */
#define CLIP(x) ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))

/**
* @brief Convert YUYV422 frame to an RGB24 frame
*
* Converts a raw YUYV422 camera frame into an RGB24 format suitable for
* JPEG compression and object detection (later extension).
* The conversion follows the BT.601 color space specification and uses 
* integer arithmetic for performance.
*
* @param yuyv  Pointer to the source YUYV422 frame
* @param rgb   Pointer to the destination RGB frame
*
* @return 0 on success, -1 on failure
*/
int convert_yuyv_to_rgb(const struct yuyv_frame *yuyv,
                        struct rgb_frame *rgb)
{
    if (!yuyv || !rgb) return -1;

    rgb->width  = yuyv->width;
    rgb->height = yuyv->height;
    rgb->size   = yuyv->width * yuyv->height * 3;

    rgb->data = malloc(rgb->size);              // Allocate memory for the pixel data
    if (!rgb->data) return -1;

    const unsigned char *src = yuyv->data;      // The data is const. You can move the pointer
    unsigned char *dst = rgb->data;

    /**   In each width iteration:
    *       - Process 2 pixels
    *       - Consumes 4 bytes of YUYV
    *       - Produces 6 bytes of RGB (2 pixels x 3 channels) */
    for (int y=0; y < yuyv->height; y++) {
        for (int x=0; x < yuyv->width; x += 2) {

            int y0 = src[0];
            int u  = src[1];
            int y1 = src[2];
            int v  = src[3];
            src += 4;

            /**
            u & v are initially stored as unsigned bytes, range: [0, 255]
            - u = 128 -> no blue shift    (neutral chroma)
            - v = 128 -> no red shift     (neutral chroma)
            Subtract 128 to recenter chroma values:
            original:    0 -> 255
            Centered:  -128 -> +127    (neutral chroma at 0 now)
            Result: d & e = signed color offsets
                - Negative -> reduces color
                - Positive -> increases color
                - 0 -> no color contribution 
            */ 
            int d = u - 128;
            int e = v - 128;

            /** Pixel 0
            Y is initially stored as unsigned byte, range: [0, 255]
            BT.601 valid luma Range: [16, 235]
                Y = 16 -> black
                Y = 235 -> white
                Subtract 16 to normalize so that black = 0
            YUYV -> RGB Conversion equation:                                    -> Floating point is slow, scale by 256 to get integer
                R = 1.164 * (Y - 16) + 1.596 * (V - 128)                        -> 298 * (Y - 16) + 409 * (V - 128)
                G = 1.164 * (Y - 16) - 0.391 * (U - 128) - 0.813 * (V - 128)    -> 298 * (Y - 16) - 100 * (U - 128) - 208 * (V - 128)
                B = 1.164 * (Y - 16) + 2.018 * (U - 128)                        -> 298 * (Y - 16) + 516 * (U - 128)
            */
            int c = y0 - 16; 
            *dst++ = CLIP((298*c + 409*e + 128) >> 8);            // Red
            *dst++ = CLIP((298*c - 100*d - 208*e + 128) >> 8);    // Green
            *dst++ = CLIP((298*c + 516*d + 128) >> 8);            // Blue

            // Pixel 1
            c = y1 - 16;
            *dst++ = CLIP((298*c + 409*e + 128) >> 8);
            *dst++ = CLIP((298*c - 100*d - 208*e + 128) >> 8);
            *dst++ = CLIP((298*c + 516*d + 128) >> 8);
        }
    }
    return 0;
}

/**
* @brief Encode RGB24 frame into JPEG format
*
* Compresses an RGB frame into a JPEG image using th libjpeg library.
* The resulting JPEG data is written to a memory buffer owned by the jpeg_frame structure.
*
* The function initializes and configures the JPEG compressor, writes scanlines
* sequentially, and finalizes compression before returning.
*
* @param rgb   Pointer to the source RGB frame
* @param jpeg  Pointer to the destination JPEG frame
*
* @return 0 on success, -1 on failure
*/
int convert_rgb_to_jpeg(const struct rgb_frame *rgb,
                        struct jpeg_frame *jpeg)
{
    struct jpeg_compress_struct cinfo;      // main JPEG compression object
    struct jpeg_error_mgr jerr;             // Error manager struct used by libjpeg

    // Link cinfo compression object to libjpeg internal error-handling system
    cinfo.err = jpeg_std_error(&jerr);

    // 1. Initializes the compressor, allocate internal memory and prep cinfo for compression
    jpeg_create_compress(&cinfo);        

    // 2. Set output destination to memory buffer
    jpeg_mem_dest(&cinfo, &jpeg->data, &jpeg->size);

    // 3. Image parameters
    cinfo.image_width = rgb->width;
    cinfo.image_height = rgb->height;
    cinfo.input_components = 3;         // # of color components in input image (RGB = 3 channels)
    cinfo.in_color_space = JCS_RGB;     // color space of input image (Input is RGB)

    // 4. Default config
    jpeg_set_defaults(&cinfo);          
    jpeg_set_quality(&cinfo, 80, TRUE);         // 80 = good balance

    // Start compressor
    jpeg_start_compress(&cinfo, TRUE);       // TRUE = write full Q-tables and Huffman tables

    // Each scanline is width * 3 bytes
    while (cinfo.next_scanline < cinfo.image_height) {

        JSAMPROW row_pointer[1];
        row_pointer[0] = &rgb->data[cinfo.next_scanline * rgb->width * 3];

        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    // Finish compression
    jpeg_finish_compress(&cinfo);

    // Cleanup
    jpeg_destroy_compress(&cinfo);

    return 0;
}