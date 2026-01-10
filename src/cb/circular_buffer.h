#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

/**
* @file circular_buffer.h
* @brief Circular buffer interface for storing pointers to JPEG frames.
*/

#include <stdint.h>
#include <stdbool.h>

/** @brief Max number of JPEG frames held in the circular buffer. */
#define BUFFER_SIZE     10

// Forward declare the JPEG frame struct
struct jpeg_frame;

/**
* @brief Circular buffer structure for storing pointers to JPEG frames.
*
* - entries: array of pointers to jpeg_frame elements
* - head: index for writing new frames (producer position)
* - tail: index for reading frames (consumer position)
*/
typedef struct CircularBuffer{
    struct jpeg_frame* entries[BUFFER_SIZE];        /**< Array of pointers to frames */ 
    uint32_t head;                                  /**< write index */ 
    uint32_t tail;                                  /**< read index */ 
} CircularBuffer;

/** Function prototypes */
void circular_buffer_init(CircularBuffer *cb);
void cb_write(CircularBuffer *cb, struct jpeg_frame* frame);
bool cb_read(CircularBuffer *cb, struct jpeg_frame** frame);

#endif  // CIRCULAR_BUFFER_H