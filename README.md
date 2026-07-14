# Pulse-X: Crowd Cheering Tool

Pulse-X is a portable battery powered cylindrical speaker engineered for high-noise outdoor environments, such as sports stadiums and open-air events. Works with wifi, bluetooth or locally with a sd card. Made this project to be used in VEX and FIRST competitions.

## Renders
<img width="686" height="403" alt="Pulse-X Hardware View" src="https://github.com/user-attachments/assets/dc31d1af-a8e4-449f-8a6b-3aa0df101d61" />

## Hardware Schematics
<img width="3000" height="2560" alt="circuit_image (6)" src="https://github.com/user-attachments/assets/5f80c291-9151-49b9-a680-37598e868892" />

## Images
<img width="1019" height="571" alt="image" src="https://github.com/user-attachments/assets/03ea0b9d-58d1-4bae-b5d7-3ef0825b037f" />

Demo Video: https://www.youtube.com/shorts/XgMtZgEX2DE
---
# Technical Overview

Pulse-X is powered by an ESP32-S3-WROOM-1. The ESP32 handles the web interface, audio streaming, and the OLED display at the same time. Audio is sent over I2S to a PCM5102A DAC OR with VHM-314, which provides much better sound quality than the built-in DAC found on many microcontrollers.

## Features

- Joins a public wifi, allowing devices to connect to it with BubbleUPnP or MediaMonkey
- Uses a PCM5102A I2S DAC for clean digital audio output.
- Uses a VHM-314 instead of built in bluetooth for smoother audio.
- 128×64 OLED display showing the current mode, volume level, playback status, and scrolling track names.
- Six physical buttons for volume control, play/pause, previous track, next track, and mode switching.
- Powered by a 3-cell Li-ion battery pack with a 3S BMS for battery protection and a buck converter to provide power to the components.
- Bluetooth, wi-fi AND sd card connectivity.
- On/Off and connection sounds
- Bluetooth module can be integrated as shown in schematics but its optional

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
- VHM-314 Bluetooth module   // OPTIONAL

| NAME | QUANTITY | PRICE PER UNIT (TL) | TOTAL (TL) | SELLER | SHIPPING (TL) | LINK |
|------|---------:|--------------------:|-----------:|--------|--------------:|------|
| LG HG2 3000 mAh 3.7V 20A 18650 Li-ion Battery | 3 | 299.00 | 897.00 | Trendyol | 59.99 | https://www.trendyol.com/lg/hg2-3000-mah-3-7-v-20a-18650-li-ion-sarj-edilebilir-pil-p-142219925 |
| 1.3" SSD1306 OLED Screen | 1 | 227.31 | 227.31 | Direnç.net | 144.00 | https://www.direnc.net/13-inc-4-pin-oled-ekran-modulu-ssd1306-i2c-beyaz |
| ESP32-S3 N16R8 | 1 | 568.26 | 568.26 | Direnç.net | 0.00 | ESP32 S3 N16R8 WiFi Bluetooth Board |
| 6x6 4.3mm Tact Button | 7 | 1.48 | 10.36 | Direnç.net | 0.00 | https://www.direnc.net/6x6-42mm-tach-buton-4-bacak-en |
| Speaker 4Ω 5W 66×66mm | 2 | 86.40 | 172.80 | Motorobit | 137.00 | http://motorobit.com/hoparlor-4-ohm-4%CE%A9-5w-66x66mm |
| MP1584 Step-Down Voltage Regulator 3A | 1 | 50.22 | 50.22 | Robo90 | 0.00 | https://www.robo90.com/mp1584-dusurucu-voltaj-regulatoru-buck-3a |
| Micro SD Card Reader | 1 | 30.69 | 30.69 | Robo90 | 0.00 | https://www.robo90.com/micro-sd-kart-okuyucu-modulu-arduino-uyumlu |
| PAM8610 Sound Booster | 1 | 83.70 | 83.70 | Robo90 | 0.00 | https://www.robo90.com/pam8610-mini-dijital-ses-yukseltici |
| PCM5102A I2S DAC Audio Decoder Module | 1 | 50.22 | 50.22 | Robo90 | 0.00 | https://www.robo90.com/pcm5102a-i2s-dac-dekoder-modul |
| 3S 20A BMS | 1 | 50.40 | 50.40 | Robolink Market | 0.00 | https://www.robolinkmarket.com/3s-20a-bms-batarya-yonetim-ve-koruma-modulu |
| 3S Li-ion and LiPo Battery Charging Module 12.6V 2A | 1 | 52.20 | 52.20 | Robolink Market | 0.00 | https://www.robolinkmarket.com/3s-lion-ve-lipo-sarj-modulu |
| KCD1-101 3-Pin Mini | 1 | 5.18 | 5.18 | Motorobit | 0.00 | https://www.motorobit.com/kcd11-3-pin-mini-on-off-anahtar-siyah |
| VHM-314 | 1 | 111.60 | 111.60 | Robo90 | 0.00 | https://www.robo90.com/bluetooth-50-mikrofonlu-mp3-stereo-muzik-modulu-ses-alicisi |
| **Components Total** |  |  | **2,310.94 TL** |  |  |  |
| **Shipping Total** |  |  | **340.99 TL** |  |  |  |
| **Grand Total** |  |  | **2,651.93 TL** |  |  |  |

---

# Software

Pulse-X can be programmed using either the Arduino IDE or ESP-IDF.
Preprogrammed versions are available at firmware/bluetooth.ino firmware/onlySD.ino firmware/wifinsd.ino 

---

# Steps To Replicate

1. 3d print all of the files given at folder 3dprintfiles with your preferred slicer.
2. Solder and connect all of the components as shown in schematics
3. Flash the firmware to the esp32
4. Charge the batteries
5. Download your sound effects and preferred musics to the sd card
6. Connect all of the 3d printed pieces together
7. Connect to the device

# AI usage
AI was used to help write the firmware for this project. AI was partly used in part selection and troubleshooting.
