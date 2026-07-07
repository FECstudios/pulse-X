#include "Arduino.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <driver/i2s.h> // Eski kararlı ve hatasız I2S sürücü kütüphanesi

// --- OLED Ekran Ayarları (SDA: 8, SCL: 9) ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Fiziksel Buton Giriş Pinleri (Kuzey'in Kontrol Paneli) ---
#define BTN_VOL_UP   3   // Ses +
#define BTN_VOL_DN   35  // Ses -
#define BTN_PLAY     36  // Oynat / Durdur (Cihaz içindeki ses akışını dondurur)
#define BTN_BT       48  // BT Mod Göstergesi
#define BTN_PREV     47  // Önceki (Görsel animasyonu sıfırlar)
#define BTN_NEXT     21  // Sonraki (Görsel animasyonu sıfırlar)

// --- Analog Ses Giriş Pinleri (VHM-314 -> S3 ADC Girişleri) ---
#define ANALOG_LEFT_PIN  1  // GPIO 1 (VHM-314 L Çıkışı)
#define ANALOG_RIGHT_PIN 2  // GPIO 2 (VHM-314 R Çıkışı)

// --- Dijital Ses Çıkış Pinleri (S3 -> PCM5102A Mor Kart) ---
#define I2S_BCK_PIN  4      // Şemandaki BCK
#define I2S_DATA_PIN 5      // Şemandaki DIN
#define I2S_LCK_PIN  6      // Şemandaki LCK (WS)

// --- Arayüz Durum Değişkenleri ---
bool isPlaying = true;
int volumePercent = 75; // Başlangıç ses seviyesi
int progress = 0;
String currentTrackName = "Pulse-X Bluetooth 5.0 Audio Stream";

// Kayan Yazı Zamanlayıcıları
int scrollX = 12;
unsigned long lastScrollTime = 0;
const int scrollSpeed = 35;
const int startDelay = 2000;
bool isDelaying = true;

unsigned long lastBtnTime = 0;
const int debounce = 250; 
bool needsRedraw = true;

// --- I2S Sürücü Kurulumu (Eski Sürüm v2.x ve v3.x Uyarı Modu Uyumlu) ---
void init_i2s_dac() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,                           // 44.1 kHz Standart CD kalitesi
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,   // 16-bit derinlik
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,   // Stereo (Sağ ve Sol Kanal)
    .communication_format = I2S_COMM_FORMAT_STAND_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,       
    .dma_buf_count = 8,                             // Seste takılma olmaması için buffer sayısı
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK_PIN,     
    .ws_io_num = I2S_LCK_PIN,      
    .data_out_num = I2S_DATA_PIN,  
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  // Sürücülerin sisteme yüklenmesi
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// --- Değişmeyen Panel Grafikleri ---
void drawStatic() {
  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 12, SSD1306_WHITE);

  // Sol (Geri) Ok Simgesi
  display.drawCircle(28, 22, 10, SSD1306_WHITE);
  display.fillTriangle(30, 18, 30, 26, 24, 22, SSD1306_WHITE);
  display.drawFastVLine(24, 18, 9, SSD1306_WHITE);

  // Orta (Oynat/Durdur) Simgesi
  display.drawCircle(64, 22, 11, SSD1306_WHITE);
  if (isPlaying) {
    display.fillRect(61, 17, 2, 10, SSD1306_WHITE);
    display.fillRect(65, 17, 2, 10, SSD1306_WHITE);
  } else {
    display.fillTriangle(61, 17, 61, 27, 69, 22, SSD1306_WHITE);
  }

  // Sağ (İleri) Ok Simgesi
  display.drawCircle(100, 22, 10, SSD1306_WHITE);
  display.fillTriangle(98, 18, 98, 26, 104, 22, SSD1306_WHITE);
  display.drawFastVLine(104, 18, 9, SSD1306_WHITE);

  // Sol Üst: Ses Yüzdesi
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(8, 6);
  display.print("V:%");
  display.print(volumePercent);

  // Sağ Üst: Bluetooth Mod Etiketi
  display.setCursor(98, 6);
  display.print("BT");

  // Alt Kısım: İlerleme Çubuğu (Progress Bar)
  display.drawRoundRect(20, 54, 88, 4, 1, SSD1306_WHITE);
  int filled = map(progress, 0, 100, 0, 86);
  display.fillRect(21, 55, filled, 2, SSD1306_WHITE);

  display.display();
}

// --- Kayan Yazı İşleme Motoru ---
void updateScrollingText() {
  display.fillRect(11, 38, 106, 12, SSD1306_BLACK);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  int16_t x1, y1;
  uint16_t trackWidth, trackHeight;
  display.getTextBounds(currentTrackName.c_str(), 0, 0, &x1, &y1, &trackWidth, &trackHeight);

  int maxVisibleWidth = 104;

  if (trackWidth <= maxVisibleWidth) {
    int staticX = (128 - trackWidth) / 2;
    display.setCursor(staticX, 40);
    display.print(currentTrackName);
  } else {
    unsigned long currentTime = millis();
    if (isDelaying) {
      if (currentTime - lastScrollTime > startDelay) {
        isDelaying = false;
        lastScrollTime = currentTime;
      }
    } else {
      if (isPlaying && currentTime - lastScrollTime > scrollSpeed) {
        scrollX--;
        lastScrollTime = currentTime;
        if (scrollX < -(int)trackWidth) {
          scrollX = 12;
          isDelaying = true;
          lastScrollTime = currentTime;
        }
      }
    }
    display.setCursor(scrollX, 40);
    display.print(currentTrackName);
    display.fillRect(1, 38, 10, 11, SSD1306_BLACK);
    display.fillRect(117, 38, 10, 11, SSD1306_BLACK);
  }
  display.display();
}

// --- Fiziksel Butonların Durum Kontrolü ---
void checkButtons() {
  unsigned long now = millis();
  if (now - lastBtnTime < debounce) return;

  if (digitalRead(BTN_VOL_UP) == LOW) {
    volumePercent = min(100, volumePercent + 5);
    needsRedraw = true;
    lastBtnTime = now;
  }
  else if (digitalRead(BTN_VOL_DN) == LOW) {
    volumePercent = max(0, volumePercent - 5);
    needsRedraw = true;
    lastBtnTime = now;
  }
  else if (digitalRead(BTN_PLAY) == LOW) {
    isPlaying = !isPlaying;
    needsRedraw = true;
    lastBtnTime = now;
  }
  else if (digitalRead(BTN_NEXT) == LOW || digitalRead(BTN_PREV) == LOW) {
    progress = 0; // Şarkı geçiş animasyonunu simüle etmek için barı sıfırla
    needsRedraw = true;
    lastBtnTime = now;
  }
}

void setup() {
  // Buton Pin Tanımlamaları ve Dahili Pull-Up Dirençleri
  pinMode(BTN_VOL_UP, INPUT_PULLUP);
  pinMode(BTN_VOL_DN, INPUT_PULLUP);
  pinMode(BTN_PLAY,   INPUT_PULLUP);
  pinMode(BTN_BT,     INPUT_PULLUP);
  pinMode(BTN_PREV,   INPUT_PULLUP);
  pinMode(BTN_NEXT,   INPUT_PULLUP);

  // S3'ün ADC okuma çözünürlüğünü 12-bit (0-4095) yapıyoruz
  analogReadResolution(12);

  // I2C Ekran Veri Yolu Başlatma
  Wire.begin(8, 9);
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;); // Ekran yoksa sistemi kilitle
  }

  display.setTextWrap(false);
  init_i2s_dac(); // I2S Donanım motorunu başlat
  drawStatic();
}

void loop() {
  checkButtons();

  // --- GERÇEK ZAMANLI SES KÖPRÜLEME MOTORU ---
  if (isPlaying) {
    // 1. Bluetooth kartının analog ses çıkışını anlık olarak oku ve merkeze çek (-2048)
    int16_t leftSample = (analogRead(ANALOG_LEFT_PIN) - 2048);
    int16_t rightSample = (analogRead(ANALOG_RIGHT_PIN) - 2048);

    // 2. Fiziksel butonlardan gelen ses yüzdesine göre sinyali ölçekle
    leftSample = (leftSample * volumePercent) / 100 * 16;
    rightSample = (rightSample * volumePercent) / 100 * 16;

    // 3. Sinyalleri sağ/sol tampon belleğe (buffer) diz
    int16_t i2sBuffer[2] = {leftSample, rightSample};
    size_t bytesWritten;
    
    // 4. Eski kararlı kütüphane fonksiyonuyla veriyi doğrudan mor karta fırlat
    i2s_write(I2S_NUM_0, i2sBuffer, sizeof(i2sBuffer), &bytesWritten, portMAX_DELAY);
  }

  if (needsRedraw) {
    drawStatic();
    needsRedraw = false;
  }

  updateScrollingText();

  // Görsel İlerleme Çubuğu Akışı
  if (isPlaying) {
    static unsigned long lastProgress = 0;
    if (millis() - lastProgress > 1500) {
      progress = (progress + 1) % 101;
      lastProgress = millis();
      needsRedraw = true;
    }
  }
}
