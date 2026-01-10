/**
* @file circular_buffer.c
* @brief Circular buffer implementation for storing JPEG frame pointers
*
* This module provides a FIFO circular buffer with overwrite-on-full behaviour.
* It is intended for producer-consumer pipelines where old frames are dropped.
*/

#include <stdint.h>
#include <stdbool.h>

#include "circular_buffer.h"
#include "image/image_encoder.h"

/**
* @brief Initialize the circular buffer
*
* Sets the head and tail indices to zero, marking the buffer as empty.
*
* @param cb Pointer to the CircularBuffer instance.
*/
void circular_buffer_init(CircularBuffer *cb) {
    cb->head = 0;
    cb->tail = 0;
}

/**
* @brief Write a JPEG frame pointer into the circular buffer
* 
* The frame pointer is stored at the current head position.
* If the buffer is full, the oldest frame is overwritten by 
* advancing the tail index.
*
* @param cb Pointer to the CircularBuffer instance.
* @param frame Pointer to the jpeg_frame to store
*
* @return void
*/
void cb_write(CircularBuffer *cb, struct jpeg_frame *frame)
{
    // Store frame pointer at current write position
    cb->entries[cb->head] = frame;

    // Advance head index (wrap around at BUFFER_SIZE)
    cb->head = (cb->head + 1) % BUFFER_SIZE;

    // If head catches up to tail, buffer was full
    // Advance tail to discard the oldes entry 
    if (cb->head == cb->tail) {
        cb->tail = (cb->tail + 1) % BUFFER_SIZE;
    }
}

/**
* @brief Read a JPEG frame pointer from the circular buffer
* 
* Retrieves the oldest frame in FIFO order.
*
* @param cb Pointer to the CircularBuffer instance.
* @param output Address of a jpeg_frame pointer to receive the frame
*
* @return true if frame successfully read
*         false if the buffer was empty
*/
bool cb_read(CircularBuffer *cb, struct jpeg_frame **output) 
{
    // Buffer is empty if head equals tail
    if (cb->head == cb->tail) {
        return false;
    }

    // Retrieve the oldest frame
    *output = cb->entries[cb->tail];

    // Advance tail index
    cb->tail = (cb->tail + 1) % BUFFER_SIZE;

    return true;
}

