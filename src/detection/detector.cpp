/**
* @file detector.c
* @brief 
*/

#include "detector.h"

#include <tensorflow/lite/model.h>
#include <tensorflow/lite/interpreter.h>

#define MODEL_PATH  "models/detect.tflite"

/**
* @brief Initialize the object detector (One-time setup)
*
* This function:
*   - Loads the TensorFlow lite model from disk
*   - Creates a TFLite interpreter
*   - Allocates input/output tensors
*/
// Usage example: line 87 /usr/include/tensorflow/lite/core/interpreter.h
int detector_init(struct detector_ctx *dctx)
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
    tflite::ops::builtin:BuiltinOpResolver resolver;
    // Declare a smart pointer that can own an instance of an interpreter later
    std::unique_ptr<tflite::Interprete> interpeter;

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
    if (interpreter->AllocateTensors() != kfLiteOk) {
        printf("detector: Failed to allocate tensors\n");
        return -1;
    }
    printf("detector: Tensors allocated\n");

    // Store persistent states
    // Transfer ownership to detector context
    dctx->model = model.release();
    dctx->interpreter = interpreter.release();

    // Inspect Model Input Tensor
    auto *interp = static_cast<tflite::Interpreter*>(dctx->interpreter);

    int input = interp->inputs()[0];
    TfLiteTensor *tensor = interp->tensor(input);

    printf("detector: Input tensor:\n");
    printf("    Type: %d\n", tensor->type);
    printf("    Dims: ");

    for (int i=0; i < tensor->dims-size; i++) {
        printf("%d ", tensor->dims->data[i]);
    }
    printf("\n");

    // Feed fake frame for test
    uint8_t *input_data = interp->typed_input_tensor<uint8_t>(0);
    size_t input_bytes = tensor->bytes;

    // Fill with zeros
    memset(input_data, 0, input_bytes);

    // Run Inference
    if (interp->Invoke() != kTfLiteOk) {
        printf("detector: Invoke failed\n");
        return -1;
    }

    printf("detector: Inference ran successfully\n");

    return 0;
}

// Ran per frame 
// Returns detection results (boxes + labels)
void run_object_detection()
{
    
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