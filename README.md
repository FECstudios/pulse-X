# Pulse-X: Crowd Cheering Tool

Pulse-X is a standalone, Wi-Fi-powered cylindrical speaker engineered for high-noise outdoor environments, such as sports stadiums and open-air events. By utilizing an independent, local Wi-Fi Access Point (AP) network, it bypasses traditional Bluetooth range limits, pairing issues, and cellular data dependencies to stream high-fidelity audio with ultra-low latency.

## Gallery
<img width="686" height="403" alt="Pulse-X Hardware View" src="https://github.com/user-attachments/assets/dc31d1af-a8e4-449f-8a6b-3aa0df101d61" />

## Hardware Schematics
<img width="1000" height="auto" alt="Pulse-X Circuit Diagram" src="https://github.com/user-attachments/assets/d92e4ddd-3754-4392-b152-778c538afc9e" />

---

## Technical Overview

Pulse-X is driven by an ESP32-S3-WROOM-1 microcontroller utilizing its dual-core processing capabilities and 8MB PSRAM to manage local Wi-Fi routing, web streaming sockets, and audio decoding simultaneously without playback stutter. 

The digital audio data is routed over an **I2S (Inter-IC Sound)** bus to a dedicated **PCM5102A DAC** module, converting it to an analog signal. This analog line-out feeds directly into a high-efficiency **PAM8610 Class-D amplifier** powered by a regulated 3S lithium battery pack ($12.6\text{V}$ peak), driving high-SPL outdoor speakers.

### Key Features
* **Zero-Infrastructure Local Streaming:** Hosts its own Wi-Fi Access Point. Devices connect directly to the speaker's local network IP to stream audio—no internet required.
* **Hi-Fi I2S Audio Pipeline:** Skips the noisy internal ADC/DAC of standard microcontrollers. Uses a dedicated 24-bit PCM5102A DAC over I2S for clean, uncompressed audio.
* **On-Board UI Dashboard:** Features a 128x64 SSD1306 OLED display mapping real-time playback statuses, dynamic volume percentages, and an automatic horizontal scrolling text engine for long track names.
* **Tactile 6-Button Control Panel:** Hardware interrupts mapped to debounced physical buttons for instantaneous Volume Up, Volume Down, Play/Pause, Mode Toggle, Previous Track, and Next Track.
* **Rugged Power Management:** Integrated 3S BMS (Battery Management System) with a heavy-duty physical toggle switch isolating the $12.6\text{V}$ high-power rail from the ultra-quiet D-SUN step-down buck converter supplying the $5\text{V}/3.3\text{V}$ logic boards.

---

## Hardware Architecture & Pin Mapping

### 1. Audio & Display Connections
| Component | Pin Function | ESP32-S3 GPIO | Notes |
| :--- | :--- | :--- | :--- |
| **PCM5102A DAC** | Bit Clock (BCK) | **GPIO 4** | I2S Serial Clock Line |
| | Data In (DIN) | **GPIO 5** | I2S Serial Data Line |
| | Left/Right Clock (LCK / WS) | **GPIO 6** | I2S Word Select Line |
| **SSD1306 OLED** | Data (SDA) | **GPIO 8** | I2C Data Line (External $4.7\text{k}\Omega$ Pull-up) |
| | Clock (SCL) | **GPIO 9** | I2C Clock Line (External $4.7\text{k}\Omega$ Pull-up) |

### 2. Control Panel Interruption Matrix
All buttons are wired to physical GND and utilize internal micro-controller pull-up configuration (`INPUT_PULLUP`).
* **Volume Up:** GPIO 3
* **Volume Down:** GPIO 35
* **Play / Pause:** GPIO 36
* **Previous Track:** GPIO 47
* **Next Track:** GPIO 21
* **Mode Switch:** GPIO 48

---

## Software Framework & Deployment

Pulse-X software infrastructure can be built via the **ESP-IDF** toolchain or the **Arduino IDE** (using the ESP32 Core v2.x/v3.x framework). 

### Essential Dependencies
Ensure you have the following libraries installed in your environment:
* `Adafruit_SSD1306` & `Adafruit_GFX` (Optimized for I2C DMA operations)
* `ESP32-audioI2S` (Wolle) or native `<driver/i2s.h>` components depending on production target.

### Production Pipeline Build Rules
1. **Buffer Calibration:** Allocate a minimum of 8 DMA buffers with a buffer length of 64 bytes to prevent local packet drop or loop underruns during peak high-bitrate playback.
2. **ADC Attenuation:** When testing alternative analog-override codebases via VHM-314 modules, explicitly configure `analogReadResolution(12)` to sample the ADC full range ($0 - 4095$) safely at $3.3\text{V}$.
3. **I2C Clock Speed:** Force the OLED display bus speed to `400000Hz` (`Wire.setClock(400000)`) inside `setup()` to keep UI rendering tasks from blocking critical audio decoding loops.

---

## License
This hardware configuration, schematics, and firmwares are open-source. Created for competitive robotics applications, custom IoT integrations, and advanced sound engineering prototyping.
