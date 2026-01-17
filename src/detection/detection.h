#ifndef DETECTOR_H
#define DETECTOR_H

/**
* @file detector.h
* @brief Public interface for the TensorFlow Lite object detection module.
*/

#include <stdint.h>

/** @brief */
#define MAX_DETECTIONS  5

/** @brief Context structure for the object detection module */
struct detector_ctx {
    void *model;                /**< tflite::FlatBufferModel* */
    void *interpreter;          /**< tflite::Interpreter* */
};

struct box {
    float xmin, ymin, xmax, ymax;
};

struct detection_result {
    int num_detections;
    struct box boxes[MAX_DETECTIONS];
    int class_ids[MAX_DETECTIONS];
    float scores[MAX_DETECTIONS];
};

/** Function Prototypes */
int detector_init(struct detector_ctx *dctx);
// void run_object_detection(struct detector_ctx *dctx);
// void draw_detections(unsigned char *rgb,
//                      int width,
//                      int height,
//                     struct detection_result *result);

#endif  // DETECTOR_H