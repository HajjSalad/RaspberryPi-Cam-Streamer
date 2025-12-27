/**
* @file detector.c
* @brief 
*
* 
*/

// Done only once
void detector_init(struct detector_ctx *dctx)
{
    // Load .tflite model 


    // Load label map


    // Allocate inference tensors


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