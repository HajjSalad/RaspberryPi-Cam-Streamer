## 📹 Embedded Camera Streaming System
A complete embedded Linux project demonstrating device driver development, IOCTL-based LED control, and real-time   
camera streaming using the V4L2 subsytem.
The project consists of:
- A **Linux kernel module** exposing custom IOCTL commands to toggle camera-status LEDs. 
- A **user-space camera client** implementing a full V4L2 streaming pipeline: device configuration, buffer
negotiation, mmap, queue/dequeue loop and frame capture.
- A simple **ffplay pipeline** to view RAW video frames streamed from the camera client.  

This project is designed for Raspberry Pi 5 hardware but can be adapted to other Linux platforms with a V4L2-compatible 
camera and GPIO-accessible LEDs, with minor adjustments for GPIO pin mapping and device paths.

## 🚀 Project Features
✅ **Kernel Module (`cam_stream.ko`)**
- Implements a character device  
- Exposes custom IOCTLs
- AAA

✅ **User-space Camera Client (`camera_client`)**
- Implements a complete V4L2 capture pipeline:
- Open device
- Close device

## 📂 Repository Structure
```
📁 pi_live_stream/
│   ├── camera_client.c       # User-space V4L2 streaming client
│   ├── camera_client.h       # Client API (buffer mgmt, IOCTL commands)
│   ├── cam_stream.c          # Kernel module (IOCTL + LEDs)
│   ├── Makefile              # Builds kernel module + client
│   └── README.md             # Documentation
```
## ⚙️ Hardware
- Raspberry Pi 5
- Logitech C270 webcam
- Two RGB LEDs connected to GPIO:
- RED LED -> Idle/error indication
- GREEN LED -> Streaming active

GPIO pins are configured in the kernel module:
```
#define GPIO_BASE        571
#define LED_RED_GPIO     (GPIO_BASE + 21)
#define LED_GREEN_GPIO   (GPIO_BASE + 20)
```
---










