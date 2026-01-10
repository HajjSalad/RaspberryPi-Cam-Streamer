#ifndef DETECTOR_H
#define DETECTOR_H

/**
* @file detector.h
* @brief Public interface for the TensorFlow Lite object detection module.
*/

#include <stdint.h>

/**
* @brief
*
*/
struct detection_ctx {
    void *model;                /**< tflite::FlatBufferModel* */
    void *interpreter;          /**< tflite::Interpreter* */
};

/** Function Prototypes */
int detector_init(struct detection_ctx *dctx);
// void run_object_detection(struct detector_ctx *dctx);
// void draw_detections(unsigned char *rgb,
//                      int width,
//                      int height,
//                     struct detection_result *result);

#endif  // DETECTOR_H