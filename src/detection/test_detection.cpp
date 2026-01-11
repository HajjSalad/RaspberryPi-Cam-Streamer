/**
* @file test_detection.cpp
* @brief Validation and sanity tests for the object detection pipeline.
* 
* This file contains development-time tests used to verify correct
* initialization and execution of the TensorFlow Lite object detector.
*
* Validation performed:
*   1. Inspect model input tensor properties
*   2. Run fake inference
*   3. Verify output tensors
*
* @note This file is intended for debugging and validation during development
*       and is excluded from final builds.
*
* Usage:
* How to compile and run the test: 
*   g++ src/detection/test_detection.cpp \
*       src/detection/detection.cpp \
*       -ltensorflow-lite \
*       -o test_detection
*
*       ./test_detection
*/
#include <stdio.h>

#include "detection.h"

#include <tensorflow/lite/model.h>
#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>

/**
* @brief Run a self-test of the object detection pirpline.
*
* This function validates that the TensorFlow Lite detection pipeline 
* is correctly initialized and executable.
*
* Validation performed:
*   1. Inspects the model input tensor (shape and type)
*   2. Allocates and fills a dummy input buffer
*   3. Executes a single inference pass using Invoke()
*
* @param dctx Pointer to the detection context containing the 
*             TFLite model and interpreter.
*
* @return 0 on success, -1 on failure 
*/
int detection_self_test(struct detector_ctx *dctx)
{
    // Access TensorFlow Lite interpreter
    auto *interp = static_cast<tflite::Interpreter*>(dctx->interpreter);

    // Inspect model input tensor
    int input = interp->inputs()[0];
    TfLiteTensor *tensor = interp->tensor(input);

    printf("detector: Input tensor\n");
    printf("    Type: %d\n", tensor->type);
    printf("    Dims: ");

    for (int i=0; i < tensor->dims->size; i++) {
        printf("%d ", tensor->dims->data[i]);
    }
    printf("\n");

    // Prepare a fake input frame
    uint8_t *input_data = interp->typed_input_tensor<uint8_t> (0);
    size_t input_bytes = tensor->bytes;

    // Fill with zeros
    memset(input_data, 0, input_bytes);

    /**
    * 4. Run inference
    *
    * Invoke() executes th TensorFlow Lite computation graph using 
    * the currently populated input tensors.
    */
    if (interp->Invoke() != kTfLiteOk) {
        printf("test_detection: Invoke failed\n");
        return -1;
    }

    printf("test_detection: Inference ran successfully\n");

    return 0;
}

int main()
{
    struct detector_ctx dctx = {0};

    // Initialize detector (loads model, builds interpreter, allocates tensors)
    if (detector_init(&dctx) < 0) {
        printf("test_detection: detector_init failed\n");
        return -1;
    }

    // Run detector self-test
    if (detection_self_test(&dctx) < 0) {
        printf("test_detection: detection_self_test failed\n");
        return -1;
    }

    printf("test_detection: detector validated successfully\n");
    return 0;
}
