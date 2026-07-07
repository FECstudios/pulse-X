#include "Arduino.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include "Audio.h" // SD kart kodunda başarıyla çalışan kütüphane

// --- OLED Ekran Ayarları (SDA: 8, SCL: 9) ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Fiziksel Buton Giriş Pinleri (Kuzey'in Kontrol Paneli) ---
#define BTN_VOL_UP   3   // Ses +
#define BTN_VOL_DN   35  // Ses -
#define BTN_PLAY     36  // Oynat / Durdur
#define BTN_MODE     48  // Kanal Değiştirme Butonu (Wi-Fi İstasyonları Arası)
#define BTN_PREV     47  // Önceki Kanal
#define BTN_NEXT     21  // Sonraki Kanal

// --- PCM5102A Mor Kart I2S Pin Ayarları ---
#define I2S_BCK_PIN  4      // Şemandaki BCK
#define I2S_DATA_PIN 5      // Şemandaki DIN
#define I2S_LCK_PIN  6      // Şemandaki LCK (WS)

// --- Wi-Fi Ağ Ayarları ---
const char* ssid     = "YOUR_WIFI_SSID";     // Kendi Wi-Fi adını yaz
const char* password = "YOUR_WIFI_PASSWORD"; // Kendi Wi-Fi şifreni yaz

// --- Ses Objesinin Tanımlanması ---
Audio audio;

// --- Wi-Fi Stream / Radyo Listesi ---
const int TOTAL_STATIONS = 3;
const char* stationList[TOTAL_STATIONS] = {
  "http://stream.revma.com/bg940511108",                // Örnek Canlı Yayın Akışı 1
  "http://icecast.ndr.de/ndr/ndr2/hamburg/mp3/128/stream.mp3", // Örnek Canlı Yayın Akışı 2
  "http://yp.shoutcast.com/sbin/tunein-station.m3u?id=1234"    // Örnek Canlı Yayın Akışı 3
};

const char* stationNames[TOTAL_STATIONS] = {
  "1. Pulse-X Stream-1",
  "2. Tribun Marsi Live",
  "3. World Bass Radio"
};

int currentStationIndex = 0;

// --- Arayüz Durum Değişkenleri ---
bool isPlaying = true;
int volumePercent = 12; // Kütüphanenin iç ses sınırı (0-21 arası)
int displayVolPercent = 60; // Ekranda %0-%100 göstermek için ölçek

String currentTrackName = "Connecting Wi-Fi...";
int scrollX = 12;
unsigned long lastScrollTime = 0;
const int scrollSpeed = 30;
const int startDelay = 1500;
bool isDelaying = true;

unsigned long lastBtnTime = 0;
const int debounce = 250; 
bool needsRedraw = true;

void drawStatic() {
  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 12, SSD1306_WHITE);

  // Sol Simgesi
  display.drawCircle(28, 22, 10, SSD1306_WHITE);
  display.fillTriangle(30, 18, 30, 26, 24, 22, SSD1306_WHITE);
  display.drawFastVLine(24, 18, 9, SSD1306_WHITE);

  // Orta Simgesi
  display.drawCircle(64, 22, 11, SSD1306_WHITE);
  if (isPlaying) {
    display.fillRect(61, 17, 2, 10, SSD1306_WHITE);
    display.fillRect(65, 17, 2, 10, SSD1306_WHITE);
  } else {
    display.fillTriangle(61, 17, 61, 27, 69, 22, SSD1306_WHITE);
  }

  // Sağ Simgesi
  display.drawCircle(100, 22, 10, SSD1306_WHITE);
  display.fillTriangle(98, 18, 98, 26, 104, 22, SSD1306_WHITE);
  display.drawFastVLine(104, 18, 9, SSD1306_WHITE);

  // Ses Göstergesi
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(8, 6);
  display.print("V:%");
  display.print(displayVolPercent);

  // Wi-Fi Modu Aktif Logosu (Sinyal gücüne göre dinamik de yapılabilir)
  display.setCursor(98, 6);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("WIFI");
  } else {
    display.print("DISC");
  }

  // İnternet akışlarında (Stream) süre belli olmadığı için canlı ses dalgası simüle ediyoruz
  display.drawRoundRect(20, 54, 88, 4, 1, SSD1306_WHITE);
  if (isPlaying && WiFi.status() == WL_CONNECTED) {
    for (int i = 22; i < 106; i += 4) {
      int pulseHeight = random(1, 4);
      display.drawFastVLine(i, 56 - pulseHeight, pulseHeight, SSD1306_WHITE);
    }
  }

  display.display();
}

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

void changeStation(int index) {
  audio.connecttohost(stationList[index]);
  currentTrackName = stationNames[index];
  scrollX = 12;
  isDelaying = true;
  isPlaying = true;
  needsRedraw = true;
}

void checkButtons() {
  unsigned long now = millis();
  if (now - lastBtnTime < debounce) return;

  if (digitalRead(BTN_VOL_UP) == LOW) {
    volumePercent = min(21, volumePercent + 1);
    displayVolPercent = map(volumePercent, 0, 21, 0, 100);
    audio.setVolume(volumePercent);
    needsRedraw = true;
    lastBtnTime = now;
  }
  else if (digitalRead(BTN_VOL_DN) == LOW) {
    volumePercent = max(0, volumePercent - 1);
    displayVolPercent = map(volumePercent, 0, 21, 0, 100);
    audio.setVolume(volumePercent);
    needsRedraw = true;
    lastBtnTime = now;
  }
  else if (digitalRead(BTN_PLAY) == LOW) {
    isPlaying = !isPlaying;
    audio.pauseResume();
    needsRedraw = true;
    lastBtnTime = now;
  }
  else if (digitalRead(BTN_NEXT) == LOW || digitalRead(BTN_MODE) == LOW) {
    currentStationIndex = (currentStationIndex + 1) % TOTAL_STATIONS;
    changeStation(currentStationIndex);
    lastBtnTime = now;
  }
  else if (digitalRead(BTN_PREV) == LOW) {
    currentStationIndex = (currentStationIndex - 1 + TOTAL_STATIONS) % TOTAL_STATIONS;
    changeStation(currentStationIndex);
    lastBtnTime = now;
  }
}

void setup() {
  pinMode(BTN_VOL_UP, INPUT_PULLUP);
  pinMode(BTN_VOL_DN, INPUT_PULLUP);
  pinMode(BTN_PLAY,   INPUT_PULLUP);
  pinMode(BTN_MODE,   INPUT_PULLUP);
  pinMode(BTN_PREV,   INPUT_PULLUP);
  pinMode(BTN_NEXT,   INPUT_PULLUP);

  // OLED Ekran I2C Başlatma
  Wire.begin(8, 9);
  Wire.setClock(400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for (;;); }
  display.setTextWrap(false);

  // İlk Bağlantı Ekranı Logu
  currentTrackName = "Connecting WiFi...";
  drawStatic();

  // Wi-Fi Başlatma
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Bağlanana kadar ekranda bekle (Maksimum 10 saniye timeout)
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    currentTrackName = "Wi-Fi Connected!";
  } else {
    currentTrackName = "Connection Failed!";
    drawStatic();
    for (;;); // Bağlantı yoksa kitle
  }

  // I2S Ses Çıkış Yapılandırması (Mor Kart Pinleri)
  audio.setPinout(I2S_BCK_PIN, I2S_LCK_PIN, I2S_DATA_PIN);
  audio.setVolume(volumePercent);

  // İlk İnternet Akışını (Stream) Başlat
  changeStation(currentStationIndex);
}

void loop() {
  audio.loop(); // Wi-Fi ses paketlerini kaçırmamak için işlemci döngüsünü sürekli besler
  checkButtons();

  // Canlı ses dalgası animasyonu için ekranı sürekli güncel tutuyoruz
  static unsigned long lastAnimUpdate = 0;
  if (millis() - lastAnimUpdate > 100) {
    needsRedraw = true;
    lastAnimUpdate = millis();
  }

  if (needsRedraw) {
    drawStatic();
    needsRedraw = false;
  }

  updateScrollingText();
}

// Mecburi kütüphane boş fonksiyonları
void audio_info(const char *info){}
void audio_id3data(const char *info){}
