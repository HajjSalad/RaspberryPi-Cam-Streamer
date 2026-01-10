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

#include "detector.h"

#include <tensorflow/lite/model.h>
#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>

/** @brief Path to the TensorFlow Lite SSD MobileNet model */
#define MODEL_PATH  "models/detect.tflite"

/**
* @brief Initialize the object detection
*
* This function:
*   - Loads the TensorFlow lite model from disk
*   - Creates a TFLite interpreter
*   - Allocates input/output tensors
*/
// Usage example: line 87 /usr/include/tensorflow/lite/core/interpreter.h
int detector_init(struct detection_ctx *dctx)
{
    // Load .tflite model 
    auto model = tflite::FlatBufferModel::BuildFromFile(MODEL_PATH);
    if (!model) {
        printf("detector: Failed to load model\n");
        return -1;
    }
    printf("detector: Model loaded\n");

    /** 
    * Create and Build Interpreter.
    * The interpreter is responsible for:
    *   - Managing tensors
    *   - Executing the computation graph  */
    // Create an instance named resolver of the class BuiltinOpResolver, which lives in the namespace tflite::ops::builtin
    // It maps operator IDs in the model (ex. CONV_2D, ADD) to concrete kernel implementations (CPU code)
    // The model says: “I need a CONV_2D”, The resolver answers: “Here’s the function that runs CONV_2D”
    tflite::ops::builtin::BuiltinOpResolver resolver;
    // Declare a smart pointer that can own an instance of an interpreter later
    std::unique_ptr<tflite::Interpreter> interpreter;

    /**InterpreterBuilder inspects the model graph
        It asks the resolver for kernels
        It allocates tensors
        It constructs an Interpreter
        It moves it into your std::unique_ptr */
    tflite::InterpreterBuilder(*model, resolver)(&interpreter);
    if(!interpreter) {
        printf("detector: Failed to create interpreter\n");
        return -1;
    }
    printf("detector: Interpreter created\n");
    /** Summary of above
    BuiltinOpResolver is a namespace-scoped class that registers built-in ops; std::unique_ptr<tflite::Interpreter> merely declares ownership — the interpreter is created later by InterpreterBuilder. */

    /** 
    * Allocate inference tensors 
    * This step:
    *   - Creates input/ouput buffers
    *   - Finalizes tensor shapes
    * Must be called before Invoke()
    */
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        printf("detector: Failed to allocate tensors\n");
        return -1;
    }
    printf("detector: Tensors allocated\n");

    // Store persistent states
    // Transfer ownership to detector context
    dctx->model = model.release();
    dctx->interpreter = interpreter.release();

    return 0;
}



// Ran per frame 
// Returns detection results (boxes + labels)
// void run_object_detection()
// {
    
// }

// /** Draw the overlying Bounding Boxes
// *   - Draws rectangles
// *   - Optionally draws labels
// *   - Modifies RGB buffer in-place
// */
// void draw_detections(unsigned char *rgb,
//                      int width,
//                      int height,
//                     struct detection_result *result)
// {

// }