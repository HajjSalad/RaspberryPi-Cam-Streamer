/**
* @file jpeg_encoder.c
* @brief JPEG encoder implementation for raw YUYV422 camera frames.
*
* Implements conversion of raw YUYV422 image buffers into JPEG-compressed
* images using libjpeg.
*/

#include <stdio.h>   
#include <stdlib.h> 
#include <setjmp.h>
#include <jpeglib.h>   

#include "jpeg_encoder.h"

/**
* @brief Convert a raw YUYV422 frame into a compressed JPEG image.
* 
* This function takes a single raw camera frame in YUYV422 format (YUV 4:2:2) 
* and compresses it into JPEG format using libjpeg. The output JPEG data is 
* written into a caller-provided memory buffer.
*
* Conversion pipeline:
*   - Camera produces YUYV422 (YUV packed format)
*   - Function converts YUYV -> RGB24 (per scanline)
*   - RGB24 is fed into the libjpeg compressor
*
* Processing Steps:
*   1. Initialize libjpeg compression object and error handler
*   2. Configure the compressor to write output into an in-memory buffer
*   3. Set image parameters (width, height, components, color space)
*   4. Apply default JPEG compression settings:
*       - Sets standard Huffman tables
*       - Prepares internal structures for scanline compression
*       - Ensures reasonable defaults for quality, quantization, and optimization
*   5. Convert YUYV422 -> RGB24 one scanline at a time
*   6. Pass RGB scanlines to the JPEG compressor
*   7. Finalize JPEG output (write end markers)
*   8. Release libjpeg resources
*
* @param[in]  yuyv_data    Pointer to raw YUYV422 image buffer.
* @param[in]  width        Width of the input image in pixels.
* @param[in]  height       Height of the input image in pixels.
* @param[out] frame        Pointer to jpeg_frame structure that will receive:
*                           - frame->buffer : allocated JPEG image buffer
*                           - frame->size : size of JPEG data in bytes
*
* @return 0 on success, non-zero on failure
*
* @note Caller is responsible for freeing frame->buffer. libjpeg allocates it.
*/
int convert_yuyv_to_jpeg(unsigned char* yuyv_data,
                         int width,
                         int height,
                         struct jpeg_frame *frame)
{
    struct jpeg_compress_struct cinfo;      // main JPEG compression object
    struct jpeg_error_mgr jerr;             // Error manager struct used by libjpeg

    // Link cinfo compression object to libjpeg internal error-handling system
    cinfo.err = jpeg_std_error(&jerr);

    // 1. Initializes the compressor, allocate internal memory and prep cinfo for compression
    jpeg_create_compress(&cinfo);        

    // 2. Set output destination to memory buffer
    jpeg_mem_dest(&cinfo, &frame->data, &frame->size);

    // 3. Image parameters
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;         // # of color components in input image (RGB = 3 channels)
    cinfo.in_color_space = JCS_RGB;     // color space of input image (Input is RGB)

    // 4. Default config
    jpeg_set_defaults(&cinfo);          
    jpeg_set_quality(&cinfo, 80, TRUE);         // 80 = good balance

    jpeg_start_compress(&cinfo, TRUE);       // TRUE = write full Q-tables and Huffman tables

    // Temporary buffer to store one RGB scanline
    unsigned char *row = malloc(width * 3);     // Each scanline = width pixels * 3 bytes (R,G, B)

    // 5. Convert YUYV -> RGB and feed scanlines into libjpeg
    while (cinfo.next_scanline < cinfo.image_height) {

        // Pointer to the start of the YUYV rows
        unsigned char *yuyv = yuyv_data + (cinfo.next_scanline * width * 2);

        // Convert pairs of pixels
        for (int x = 0; x < width; x += 2) {

            int y0 = yuyv[0];
            int u = yuyv[1];
            int y1 = yuyv[2];
            int v = yuyv[3];

            // Convert YUV to RGB (BT.601 standard)
            #define CLIP(x) ( (x)<0 ? 0 : ( (x)>255 ? 255 : (x) ) )

            int c = y0 - 16;
            int d = u - 128;
            int e = v - 128;

            // Pixel 0
            int r0 = CLIP((298*c + 409*e + 128) >> 8);
            int g0 = CLIP((298*c - 100*d - 208*e + 128) >> 8);
            int b0 = CLIP((298*c + 516*d + 128) >> 8);

            // Pixel 1
            c = y1 - 16;
            int r1 = CLIP((298*c + 409*e + 128) >> 8);
            int g1 = CLIP((298*c - 100*d - 208*e + 128) >> 8);
            int b1 = CLIP((298*c + 516*d + 128) >> 8);

            // Store RGB results into row buffer correctly
            row[(x * 3) + 0] = r0;
            row[(x * 3) + 1] = g0;
            row[(x * 3) + 2] = b0;

            row[(x * 3) + 3] = r1;
            row[(x * 3) + 4] = g1;
            row[(x * 3) + 5] = b1;

            // Move to the next YUYV block (4 bytes)
            yuyv += 4;
        }

        // Feed one scanline to libjpeg
        JSAMPROW row_pointer[1] = { row };
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    // Finish compression - Writes end-of-image marker, flushes buffers, finalizes memory
    jpeg_finish_compress(&cinfo);

    // Cleanup libjpeg resources
    jpeg_destroy_compress(&cinfo);

    free(row);

    return 0;
}


