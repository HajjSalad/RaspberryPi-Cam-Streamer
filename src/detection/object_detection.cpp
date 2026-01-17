/**
* @file detection.c
* @brief TensorFlow Lite–based object detection module.
*
* This file implements the object detection subsystem, including:
*   1. One-time initialization for the TensorFlow Lite model and interpreter
*   2. Running inference on RGB frames
*   3. Rendering detection results (bounding boxes and labels) onto frames
*/

#include <cstdio>
#include <memory>
#include <cstring>

#include "detection.h"

#include <tensorflow/lite/model.h>
#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>

/** @brief Path to the TensorFlow Lite SSD MobileNet model */
#define MODEL_PATH  "src/detection/models/detect.tflite"

// Static function prototypes
static int resize_rgb_frame_nn();
static void draw_box(struct rgb_frame *rgb,
                     int x0, int y0,
                     int x1, int y1,
                     uint8_t r, uint8_t g, uint8_t b);
static void draw_pixel(struct rgb_frame *rgb,
                       int x, int y,
                       uint8_t r, uint8_t g, uint8_t b);

/**
* @brief Initialize the object detector
*
* This function:
*   1. Loads the TensorFlow lite model from disk
*   2. Creates a TFLite interpreter
*   3. Allocates input/output tensors
*
* @param dctx Pointer to the detection context containing the 
*             TFLite model and interpreter.
*
* @note See usage example around line 87 in 
*       /usr/include/tensorflow/lite/core/interpreter.h
*/
int detector_init(struct detector_ctx *dctx)
{
    // 1. Load .tflite model 
    auto model = tflite::FlatBufferModel::BuildFromFile(MODEL_PATH);
    if (!model) {
        printf("detection: Failed to load model\n");
        return -1;
    }
    printf("detection: Model loaded\n");
 
    /**
    * 2. Create and build the TensorFlow Lite interpreter.
    * 
    * The interpreter is responsible for:
    *   1. Managing tensor lifetimes
    *   2. Executing the computation graph
    *
    * The BuiltinOpResolver maps operator identifiers in the model 
    * (ex. CONV_2D, ADD) to concrete kernel implementations (CPU code).
    * The model specifies required operators, and the resolver provides
    * the corresponding execution functions. The model says: “I need a 
    * CONV_2D”, The resolver answers: “Here’s the function that runs CONV_2D”
    *
    * The InterpreterBuilder:
    *   - Inspects the model graph
    *   - Resolves required operators via the resolver
    *   - Constructs an interpreter instance
    *   - Assigns the interpreter ownership to a std::unique_ptr
    */
    std::unique_ptr<tflite::Interpreter> interpreter;   // Smart pointer that can own an instance of an interpreter later
    tflite::ops::builtin::BuiltinOpResolver resolver;
    tflite::InterpreterBuilder(*model, resolver)(&interpreter);
    if(!interpreter) {
        printf("detection: Failed to create interpreter\n");
        return -1;
    }
    printf("detection: Interpreter created\n");
  
    /** 
    * 3. Allocate inference tensors 
    * 
    * This step:
    *   - Creates input and ouput tensor buffers
    *   - Finalizes tensor shapes and memory layout
    */
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        printf("detection: Failed to allocate tensors\n");
        return -1;
    }
    printf("detection: Tensors allocated\n");

    // Transfer ownership to detection_ctx
    dctx->model = model.release();
    dctx->interpreter = interpreter.release();

    return 0;
}

/**
* @brief Run object detection on a single frame.
*
* This function performs inference using the TensorFlow Lite model
* in the detector context.
*
* Steps:
*   1. Retrieve the input tensor from the interpreter
*   2. Resize the RGB frame and place it in the input tensor
*   3. Invoke the interpreter to run inference
*   4. Read output tensors (bounding boxes, class IDs, confidence scores)
*   5. Store the results into `detection_result` struct
*
* Bounding boxes normalized coordinates:
*   [ymin, xmin, ymax, xmax]
*   Values in the range [0.0, 1,0]
*
* @param dctx   Pointer to the initialized detector context
* @param rgb  Pointer to the RGB frame to run detection on
* @param result Pointer to the struct to store the detection outputs
*/
int run_object_detection(struct detector_ctx *dctx,
                         struct rgb_frame *rgb,
                         struct detection_result *result)
{
    if (!dctx || !dctx->interpreter) {
        printf("run_object_detection: Detector not initialized\n");
        return -1;
    }
    if (!rgb || !result) {
        printf("run_object_detection: Invalid input/output pointers\n");
        return -1;
    }

    // Access the interpreter
    auto *interp = static_cast<tflite::Interpreter*>(dctx->interpreter);

    // 1. Get input tensor
    int input_index = interp->inputs()[0];
    TfLiteTensor *input_tensor = interp->tensor(input_index);

    // 2. Resize the frame and place into model input tensor (300x300 RGB)
    uint8_t *input_data = interp->typed_input_tensor<uint8_t>(0);

    if (resize_rgb_frame_nn(frame, input_data, 300, 300) != 0) {
        printf("run_object_detection: frame resize failed\n");
        return -1;
    }

    // 3. Run inference
    if (interp->Invoke() != kTfLiteOk) {
        printf("run_object_detection: Inference failed\n");
        return;
    }

    // 4. Read output tensors: boxes, class IDs, scores, detection counts   // Shape of output tensor -> how many elements
    float *boxes = interp->typed_output_tensor<float>(0);                   // [num,4]
    float *class_ids = interp->typed_output_tensor<float>(1);               // [num]
    float *scores = interp->typed_output_tensor<float>(2);                  // [num]
    int *num_detections = interp->typed_output_tensor<int>(3);              // [1]
    
    // 5. Store the results
    result->num_detections = num_detections[0];

    for (int i=0; i < result->num_detections; i++) {
        // [ymin, xmin, ymax, xmax]
        result->boxes[i].ymin = boxes[i*4 + 0];
        result->boxes[i].xmin = boxes[i*4 + 1];
        result->boxes[i].ymax = boxes[i*4 + 2];
        result->boxes[i].xmax = boxes[i*4 + 3];
        result->class_ids[i] = (int)class_ids[i];
        result->scores[i] = scores[i];
    }

    printf("run_detection: Detected %d objects\n", result->num_detections);

    return result->num_detections;
}

// Later
static int resize_rgb_frame_nn(const struct rgb_frame *src,
                               uint8_t *dst,
                               int dst_w,
                               int dst_h)
{
    if (!src || !src->data || !dst) {
        return -1;
    }

    const int src_w = src->width;       // 640
    const int src_h = src->height;      // 480
    const int channels = 3;

    /**
    * Reverse-engineer: Start with the desired size and fill from the source
    */
    for (int y=0; y < dst_h; y++) {
        for (int x=0; x < dst_w; x++) {

            // Map destination pixel to source pixel
            int src_x = x * src_w / dst_x;
            int src_y = y * src_y / dst_y;

            int src_idx = (src_y * src_w + src_x) * channels;
            int dst_idx = (y * dst_w + x) * channels;

            dst[dst_idx + 0] = src->data[src_idx + 0];  // R
            dst[dst_idx + 1] = src->data[src_idx + 1];  // G
            dst[dst_idx + 2] = src->data[src_idx + 2];  // B
        }
    }
    return 0;
}

/** 
* @brief Draw detected bounding boxes onto an RGB frame.
*   
* Converts normalized bounding box coordinates ([0.0, 1.0]) to pixel
* coordinates and renders rectangular overlays onto the RGB frame.
* All coordinates are clamped to frame bounds.
* The RGB frame is modified in place.
*
* @param rgb    Pointer to the RGB frame to draw on
* @param result Pointer to detection results containing bounding boxes
*/
void draw_detections(struct rgb_frame *rgb,
                     struct detection_result *result)
{
    if (!rgb || !rgb->data || !result){
        return;
    }

    for (int i=0; i < result->num_detections; i++) 
    {
        // Convert normalized coordinates to pixel coordinates
        int xmin = (int)(result->boxes[i].xmin * rgb->width);
        int ymin = (int)(result->boxes[i].ymin * rgb->height);
        int xmax = (int)(result->boxes[i].xmax * rgb->width);
        int ymax = (int)(result->boxes[i].ymax * rgb->height);

        // Clamp to frame bounds
        if (xmin < 0) xmin = 0;
        if (ymin < 0) ymin = 0;
        if (xmax >= rgb->width)  xmax = rgb->width - 1;
        if (ymax >= rgb->height) ymax = rgb->height - 1;

        // Draw bounding box (red)
        draw_box(rgb, xmin, ymin, xmax, ymax, 255, 0, 0);
    }
}

/**
* @brief Draw a rectangular bounding box on an RGB frame.
*
* Draws a rectangle defined by pixel coordinates by rendering 
* its four edges directly into the RGB buffer.
*
* @param rgb Pointer to the RGB frame to draw on
* @param x0  Left coordinate (pixels)
* @param y0  Top coordinate (pixels)
* @param x1  Right coordinate (pixels)
* @param y1  Bottom coordinate (pixels)
* @param r   Red color component
* @param g   Green color component
* @param b   Blue color component
*/
static void draw_box(struct rgb_frame *rgb,
                      int x0, int y0,
                      int x1, int y1,
                      uint8_t r, uint8_t g, uint8_t b) 
{
    // Iterate from xmin to xmax to draw the horizontal edges
    for (int x=x0; x <= x1; x++) {
        draw_pixel(rgb, x, y0, r, g, b);      // top edge
        draw_pixel(rgb, x, y1, r, g, b);      // bottom edge
    }

    // Iterate from ymin to ymax to draw the vertical edges
    for (int y=y0; y <= y1; y++) {
        draw_pixel(rgb, x0, y, r, g, b);      // left edge
        draw_pixel(rgb, x1, y, r, g, b);      // right edge
    }
}

/**
* @brief Overwrite a single pixel in an RGB frame.
*
* Writes the specified RGB color to pixel coordinates (x,y)
* in an interleaved RGB buffer.
*
* Memory layout:
*   - Each row starts at: rgb->data + (y * rgb->stride)
*   - Each pixel within a row occupies 3 bytes (R, G, B)
*   - Pixel (x, y) starts at:
*       rgb->data + (y * rgb->stride) + (x * 3)
*
* Coordinates outside the frame bounds are ignored.
*
* @param rgb Pointer to the RGB frame
* @param x   X coordinate (column index, pixels)
* @param y   Y coordinate (row index, pixels)
* @param r   Red color component
* @param g   Green color component
* @param b   Blue color component
*/
static inline void draw_pixel(struct rgb_frame *rgb,
                              int x, int y,
                              uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || x >= rgb->width || y < 0 || y >= rgb->height) {
        return;
    }

    // Go to row y, pixel x
    uint8_t *p = rgb->data + y * rgb->stride + x * 3;
    
    p[0] = r;
    p[1] = g;
    p[2] = b;
}