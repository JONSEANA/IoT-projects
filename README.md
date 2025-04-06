# 📦 IoT Project – Group 22  
**Smart Parcel Detector System**

---

## 📝 Introduction

This project implements a **Smart Parcel Detector System** designed to secure packages delivered to residential addresses.  
It leverages the built-in sensors of the **M5StickC Plus** microcontroller to detect parcel presence and nearby human motion, enabling real-time monitoring and alerts.
A **Raspberry Pi** is also configured to host a local server to be accessed to see updated images captured.

---

## 🧰 Hardware & Software Prerequisites

### 🔧 Hardware

- **2× Raspberry Pi 4**
  - One acts as the **controller node**
  - One acts as an **NFS file server** (for remote storage of captured images)
- **1× Pi 4 Camera Module**
  - Captures images or video of delivery events
- **4× M5StickC Plus**
  - ESP32-based microcontrollers used for sensing and display
- **1× PIR Motion Sensor**
  - Detects motion to trigger camera capture and alerts

---

### 💻 Software

- **Arduino IDE**
  - With the following libraries installed:
    - `M5StickCPlus.h`
    - `WiFi.h`
    - `WebServer.h`
    - Any relevant PIR sensor or button libraries

- **Python 3.10**
  - Required packages (install via pip):
    ```bash
    pip install flask opencv-python numpy requests
    ```

- **NFS (Network File System) Setup**
  - Configure one Raspberry Pi as the NFS **server**
  - Mount the shared filesystem on the **camera Pi** to enable centralized storage

---

## 🖧 Project Topology

![Project Topology](https://github.com/user-attachments/assets/d88cc9e7-2768-42f8-b5f4-8f3e7868dd4e)

---

## 📌 Summary

This system enables:
- Real-time parcel detection using onboard sensors
- Motion-triggered image capture and storage
- Secure data offloading via NFS
- Lightweight deployment using M5StickC Plus and Raspberry Pi

---

> Built with ❤️ by Group 22  

