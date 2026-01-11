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

/**
* @brief Initialize the object detection
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
*   2. Copy the frame data (RGB) into the input tensor
*   3. Invoke the interpreter to run inference
*   4. Read output tensors (bounding boxes, class IDs, confidence scores)
*   5. 
*
* @param dctx Pointer to the initialized detector context
*/
void run_object_detection(struct detector_ctx *dctx)
{
    if(!dctx || !dctx->interpreter) {
        printf("run_object_detection: Detector not initialized\n");
        return;
    }

    // Access the interpreter
    auto *interp = static_cast<tflite::Interpreter*>(dctx->Interpreter);

    // 1. Get input tensor
    int input_index = interp->inputs()[0];
    TfLiteTensor *input_index = interp->tensor(input_index);

    // 2. Copy frame data into input tensor

    // 3. Run inference
    if (interp->Invoke() != kTfLiteOk) {
        printf("run_object_detection: Inference failed\n");
        return;
    }

    // 4. Read output tensors
    
    // 5. Store the results

}

/** Draw the overlying Bounding Boxes
*   - Draws rectangles
*   - Optionally draws labels
*   - Modifies RGB buffer in-place
*/
void draw_detections(unsigned char *rgb,
                     int width,
                     int height,
                    struct detection_result *result)
{

}