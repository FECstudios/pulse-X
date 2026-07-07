# Pulse-X: Crowd Cheering Tool

Pulse-X is a portable battery powered cylindrical speaker engineered for high-noise outdoor environments, such as sports stadiums and open-air events. By utilizing an independent, local Wi-Fi Access Point (AP) network, it bypasses traditional Bluetooth range limits and pairing issues to stream high-definition audio with ultra-low latency.

## Renders
<img width="686" height="403" alt="Pulse-X Hardware View" src="https://github.com/user-attachments/assets/dc31d1af-a8e4-449f-8a6b-3aa0df101d61" />

## Hardware Schematics
<img width="1000" height="auto" alt="Pulse-X Circuit Diagram" src="https://github.com/user-attachments/assets/d92e4ddd-3754-4392-b152-778c538afc9e" />

## Images

---
# Technical Overview

Pulse-X is powered by an ESP32-S3-WROOM-1. The ESP32 handles the web interface, audio streaming, and the OLED display at the same time. Audio is sent over I2S to a PCM5102A DAC, which provides much better sound quality than the built-in DAC found on many microcontrollers.

## Features

- Creates its own Wi-Fi access point, allowing devices to connect directly to the speaker without an internet connection.
- Uses a PCM5102A I2S DAC for clean digital audio output.
- 128×64 OLED display showing the current mode, volume level, playback status, and scrolling track names.
- Six physical buttons for volume control, play/pause, previous track, next track, and mode switching.
- Powered by a 3-cell Li-ion battery pack with a 3S BMS for battery protection and a buck converter to provide power to the components.

---

# Hardware Connections

## Main Components

- ESP32-S3 N16R8 Development Board
- 1.3" SSD1306 OLED Display (I2C)
- PCM5102A I2S DAC Audio Decoder Module
- PAM8610 Stereo Audio Amplifier
- VHM-314 Bluetooth 5.0 Audio Receiver Module
- Micro SD Card Reader Module
- 2 × 4 Ω 5 W Speakers (66 × 66 mm)
- 7 × 6 × 6 mm Tactile Push Buttons (4.3 mm Height)
- MP1584 3A Step-Down (Buck) Voltage Regulator
- 3 × LG HG2 3000 mAh 3.7 V 20A 18650 Li-ion Batteries
- 3S 20A Battery Management System (BMS)
- 3S Li-ion/LiPo 12.6 V 2A Charging Module
- KCD1-101 3-Pin Mini Power Switch


---

# Software

Pulse-X can be programmed using either the Arduino IDE or ESP-IDF.
Preprogrammed versions are available at firmware/bluetooth.ino firmware/onlySD.ino firmware/wifiOnly.ino 

---

# Steps To Replicate

1. 3d print all of the files given at folder 3dprintfiles with your preferred slicer.
2. Solder and connect all of the components as shown in schematics
3. Flash the firmware to the esp32
4. Charge the batteries
5. Connect all of the 3d printed pieces together and you are good to go
