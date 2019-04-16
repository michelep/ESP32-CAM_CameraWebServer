# ESP32-CAM CameraWebServer

Use ESP32-CAM module (cheap ESP32 with Camera and microSD slot) as a security camera, self-powered and independent from the net. With a PIR module connected to GPIO16, it detects movement and take 
a picture, saved to the miniSD card.

*** NOT READY FOR PRODUCTION ***

## How to build

This code is Arduino SDK compatible. Need ESP32 framework for Arduino (https://github.com/espressif/arduino-esp32) and working Arduino ecosystem to be build.

Please remember to setup compiler with:
- Board type: ESP32 Wrover Module
- Partition scheme: Huge APP (3MB No OTA)

v0.0.1 16 Apr 2019