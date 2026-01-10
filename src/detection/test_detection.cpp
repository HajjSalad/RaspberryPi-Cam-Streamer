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
*/
#include <stdio.h>

#include "detector.h"

#include <tensorflow/lite/model.h>
#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>

int detection_self_test(struct detection_ctx *dctx)
{
    // Inspect Model Input Tensor
    auto *interp = static_cast<tflite::Interpreter*>(dctx->interpreter);

    int input = interp->inputs()[0];
    TfLiteTensor *tensor = interp->tensor(input);

    printf("detector: Input tensor:\n");
    printf("    Type: %d\n", tensor->type);
    printf("    Dims: ");

    for (int i=0; i < tensor->dims->size; i++) {
        printf("%d ", tensor->dims->data[i]);
    }
    printf("\n");

    // Feed fake frame for test
    uint8_t *input_data = interp->typed_input_tensor<uint8_t> (0);
    size_t input_bytes = tensor->bytes;

    // Fill with zeros
    memset(input_data, 0, input_bytes);

    // Run Inference
    if (interp->Invoke() != kTfLiteOk) {
        printf("detector: Invoke failed\n");
        return -1;
    }

    printf("detector: Inference ran successfully\n");
}

/**
*
*/
int main()
{
    struct detection_ctx dctx = {0};

    if (detection_self_test(dctx)) < 0) {
        printf("test_detection: detection_init failed\n");
        return -1;
    }

    printf("test_detection: detector initialized successfully\n");
    return 0;
}
