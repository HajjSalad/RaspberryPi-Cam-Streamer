## 📸 Real-Time Camera Streaming with Object Detection
A real-time camera streaming system built on a Raspberry Pi that integrates a custom Linux kernel module, a multithreaded user-space capture pipeline, MJPEG HTTP streaming, and optional on-device object detection using TensorFlow Lite.

This project demonstrates end-to-end system design across kernel space and user space. It combines Linux interfaces (V4L2, IOCTL, MMAP) with concurrent data pipelines and computer vision inference.

[Additional Project Notes on Notion](https://www.notion.so/hajjsalad/Pi-Camera-Streamer-Overall-Project-Notes-2cca741b5aab80cf8412cb5dc12558e8)

### 🌿 Branches and 
- `main` - Stable, fully integrated version of the project
- `stream` - Core camera capture and MJPEG streaming pipeline
- `stream_detect` - Streaming pipeline with on-device object detection
- `gh-pages` - Generated documentation hosted via github pages

### 📜 Documentation
The project includes **comprehensive Doxygen documentation** covering:
- Modules
- Functions
- Classes and detailed usage    
👉 Explore the generated docs: [Doxygen Documentation](https://hajjsalad.github.io/RaspberryPi-Cam-Streamer/html/index.html)

### 🗝️ Key Features  
✅ **Custom Linux Kernel Module**  [Notes on Notion](https://www.notion.so/hajjsalad/Cam-Stream-Kernel-Module-2cca741b5aab80e1bddbe204e5e99eae)  

- Character device exposing camera and LED controls via IOCTL
- Clean kernel ↔ user-space interface
- Runtime camera status indication via GPIO

✅ **V4L2-Based Camera Pipeline**  [Notes on Notion](https://www.notion.so/hajjsalad/V4L2-Streaming-Pipeline-2cca741b5aab80be8b30e62d9311b929)

- Camera configuration and format negotiation
- Buffer allocation and memory mapping (MMAP)
- Continuous frame capture and re-queuing

✅ **Multithreaded Producer-Consumer Architecture**
- Producer thread captures frames from the camera
- Consumer thread streams frames to HTTP clients
- Lock-protected circular buffer

✅ **MJPEG HTTP Streaming**  [Notes on Notion](https://www.notion.so/hajjsalad/MJPEG-HTTP-Streaming-2cca741b5aab80d9ab6beddf8d86db00)

- Lightweight HTTP server
- Multipart MJPEG streaming to browsers

✅ **Real-Time Object Detection**  [Notes on Notion](https://www.notion.so/hajjsalad/Object-Detection-2d2a741b5aab80ac958fc72ffb4de8a4)
- TensorFlow Lite inference on captured frames
- Designed for edge deployment
---
### 🧶 Threading Model
- Producer Thread
  - Continously capture frames using V4L2
  - Converts raw frames and pushes them into a circular buffer
  - Signals frame availability using a semaphore
- Consumer Thread
  - Waits on the semaphore for available frames
  - Retrieves JPEG frames from the circular buffer
  - Streams JPEG frames to connected HTTP clients   
  
This design allows for **producer thread** to run continously, while a new **consumer thread** is spawned per client.

### 🏗️ High Level Flow
![Block Diagram](./Pi_cam_stream_Block_diagram.png)
---
### ⚙️ Hardware
- **Raspberry Pi 5** - primary embedded platform for kernel and user-space execution
- **Logitech C270 USB webcam** - V4L2-compatible video capture device
- **GPIO-connected RGB LED** - real-time system status indication
  - RED: idle state or error condition
  - GREEN: active camera streaming

### 🧱 Build and Run
- `make module`: Build the kernel module  
- `make user`: Build the user-space application  
- `make`: Build both the kernel module & user-space application  
- `sudo insmod kernel/cam_stream.ko`: Insert the kernel module  
- `sudo ./camera_client`: Start the camera streaming application  
- `http://<raspberry-pi-ip>/stream`: Open broswer and view the stream  

### 📂 Repository Structure
```
📁 pi_live_stream/
│
├── docs/                     # Doxygen-generated documentation
│
├── kernel/                   # Linux kernel module
│   ├── cam_stream.c          # Character device + ioctl implementation
│   ├── cam_stream_ioctl.h    # Shared ioctl interface (kernel ↔ user)
│   └── Makefile              # Kernel module build rules
│
├── src/                      # User-space application
│   ├── camera/               # V4L2 camera capture & buffer management
│   │   ├── camera.c
│   │   └── camera.h
│   │
│   ├── cb/                   # Lock-protected circular buffer
│   │   ├── circular_buffer.c
│   │   └── circular_buffer.h
│   │
│   ├── detection/            # Real-time object detection (TFLite)
│   │   ├── detection.cpp
│   │   ├── detection.h
│   │   └── models/
│   │       └── detect.tflite
│   │
│   ├── http/                 # HTTP server + MJPEG streaming
│   │   ├── http_server.c
│   │   ├── http_server.h
│   │   ├── mjpeg_stream.c
│   │   └── mjpeg_stream.h
│   │
│   ├── image/                # Image processing & encoding
│   │   ├── image_encoder.c
│   │   ├── image_encoder.h
│   │   ├── image_processor.c
│   │   └── image_processor.h
│   │
│   └── main.c                # Application entry point & thread orchestration
│
├── README.md                 # Project overview & usage
└── Makefile                  # Builds kernel module and user-space client
```









