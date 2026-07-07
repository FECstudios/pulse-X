#include "Arduino.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>
#include "Audio.h"
#include <vector>
#include <algorithm>

// --- OLED Ekran Ayarları (SDA: 8, SCL: 9) ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Fiziksel Buton Giriş Pinleri (Kuzey'in Kontrol Paneli) ---
#define BTN_VOL_UP   3   // Ses +
#define BTN_VOL_DN   35  // Ses -
#define BTN_PLAY     36  // Oynat / Durdur
#define BTN_MODE     48  // Çalma Modu Değiştir (Normal / Shuffle / Repeat All / Repeat One)
#define BTN_PREV     47  // Önceki Şarkı
#define BTN_NEXT     21  // Sonraki Şarkı

// --- SD Kart SPI Pin Ayarları ---
#define SD_CS_PIN    10
#define SPI_MOSI_PIN 11
#define SPI_SCK_PIN  12
#define SPI_MISO_PIN 13

// --- PCM5102A Mor Kart I2S Pin Ayarları ---
#define I2S_BCK_PIN  4      
#define I2S_DATA_PIN 5      
#define I2S_LCK_PIN  6      

Audio audio;

// --- Gelişmiş Çalma Modları ---
enum PlayMode { NORMAL, SHUFFLE, REPEAT_ALL, REPEAT_ONE };
PlayMode currentMode = NORMAL;

// --- Dinamik Şarkı Listesi Hafızası ---
std::vector<String> playlist;
std::vector<int> shuffleOrder;
int currentTrackIndex = 0;
int shufflePosition = 0;

bool isPlaying = true;
int volumePercent = 14; 
int displayVolPercent = 70; 

String currentTrackName = "Scanning SD...";
int scrollX = 12;
unsigned long lastScrollTime = 0;
const int scrollSpeed = 30;
const int startDelay = 1500;
bool isDelaying = true;

unsigned long lastBtnTime = 0;
const int debounce = 250; 
bool needsRedraw = true;

// --- SD Kartı Alt Klasörlerle Birlikte Otomatik Tarama (Recursive) ---
void scanSDCard(File dir) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      break; // Taranacak dosya kalmadı
    }
    
    if (entry.isDirectory()) {
      scanSDCard(entry); // Alt klasör bulursa içine gir (Recursive)
    } else {
      String fileName = entry.name();
      if (fileName.endsWith(".mp3") || fileName.endsWith(".MP3")) {
        // Tam dosya yolunu listeye ekle
        playlist.push_back(String(entry.path()));
      }
    }
    entry.close();
  }
}

// --- Karıştırma (Shuffle) Sırasını Oluşturma ---
void generateShuffleOrder() {
  shuffleOrder.clear();
  for (int i = 0; i < playlist.size(); i++) {
    shuffleOrder.push_back(i);
  }
  // Rastgele karıştır
  for (int i = shuffleOrder.size() - 1; i > 0; i--) {
    int j = random(0, i + 1);
    std::swap(shuffleOrder[i], shuffleOrder[j]);
  }
  shufflePosition = 0;
}

// --- Şarkı İsmini Temizleme (.mp3 kaldırır, klasör yollarını eler) ---
String getCleanTrackName(String path) {
  int lastSlash = path.lastIndexOf('/');
  String name = (lastSlash >= 0) ? path.substring(lastSlash + 1) : path;
  if (name.endsWith(".mp3")) name.remove(name.length() - 4);
  if (name.endsWith(".MP3")) name.remove(name.length() - 4);
  return name;
}

void playTrack(int index) {
  if (playlist.empty()) return;
  
  audio.connecttoFS(SD, playlist[index].c_str());
  currentTrackName = getCleanTrackName(playlist[index]);
  
  scrollX = 12;
  isDelaying = true;
  isPlaying = true;
  needsRedraw = true;
}

void nextTrack() {
  if (playlist.empty()) return;

  if (currentMode == SHUFFLE) {
    shufflePosition = (shufflePosition + 1) % shuffleOrder.size();
    currentTrackIndex = shuffleOrder[shufflePosition];
  } else {
    currentTrackIndex = (currentTrackIndex + 1) % playlist.size();
  }
  playTrack(currentTrackIndex);
}

void prevTrack() {
  if (playlist.empty()) return;

  if (currentMode == SHUFFLE) {
    shufflePosition = (shufflePosition - 1 + shuffleOrder.size()) % shuffleOrder.size();
    currentTrackIndex = shuffleOrder[shufflePosition];
  } else {
    currentTrackIndex = (currentTrackIndex - 1 + playlist.size()) % playlist.size();
  }
  playTrack(currentTrackIndex);
}

void drawStatic() {
  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 12, SSD1306_WHITE);

  // Buton Grafikleri
  display.drawCircle(28, 22, 10, SSD1306_WHITE);
  display.fillTriangle(30, 18, 30, 26, 24, 22, SSD1306_WHITE);
  display.drawFastVLine(24, 18, 9, SSD1306_WHITE);

  display.drawCircle(64, 22, 11, SSD1306_WHITE);
  if (isPlaying) {
    display.fillRect(61, 17, 2, 10, SSD1306_WHITE);
    display.fillRect(65, 17, 2, 10, SSD1306_WHITE);
  } else {
    display.fillTriangle(61, 17, 61, 27, 69, 22, SSD1306_WHITE);
  }

  display.drawCircle(100, 22, 10, SSD1306_WHITE);
  display.fillTriangle(98, 18, 98, 26, 104, 22, SSD1306_WHITE);
  display.drawFastVLine(104, 18, 9, SSD1306_WHITE);

  // Sol Üst: Ses Seviyesi
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(8, 6);
  display.print("V:%");
  display.print(displayVolPercent);

  // Sağ Üst: Dinamik Şarkı Sayacı (Örn: 12/84) ve Mod Simgesi
  display.setCursor(82, 6);
  if (playlist.empty()) {
    display.print("00/00");
  } else {
    int displayIndex = (currentMode == SHUFFLE) ? shufflePosition + 1 : currentTrackIndex + 1;
    if (displayIndex < 10) display.print("0");
    display.print(displayIndex);
    display.print("/");
    display.print(playlist.size());
  }

  // Mod Bildirimi Ekranda Küçük Yazı
  display.setCursor(12, 18);
  if (currentMode == SHUFFLE) display.print("SHUF");
  else if (currentMode == REPEAT_ALL) display.print("R_ALL");
  else if (currentMode == REPEAT_ONE) display.print("R_ONE");
  else display.print("NORM");

  // İlerleme Çubuğu
  display.drawRoundRect(20, 54, 88, 4, 1, SSD1306_WHITE);
  uint32_t audioLen = audio.getAudioFileDuration();
  uint32_t audioCur = audio.getAudioCurrentTime();
  int progress = 0;
  if (audioLen > 0) progress = (audioCur * 100) / audioLen;
  int filled = map(progress, 0, 100, 0, 86);
  display.fillRect(21, 55, filled, 2, SSD1306_WHITE);

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
  else if (digitalRead(BTN_NEXT) == LOW) {
    nextTrack();
    lastBtnTime = now;
  }
  else if (digitalRead(BTN_PREV) == LOW) {
    prevTrack();
    lastBtnTime = now;
  }
  else if (digitalRead(BTN_MODE) == LOW) {
    // Çalma modunu döngüsel olarak değiştir
    currentMode = (PlayMode)((currentMode + 1) % 4);
    if (currentMode == SHUFFLE) {
      generateShuffleOrder();
    }
    needsRedraw = true;
    lastBtnTime = now;
  }
}

void setup() {
  // Rastgele sayı üretecini boş analog pinden besle (Shuffle için hayati)
  randomSeed(analogRead(0));

  pinMode(BTN_VOL_UP, INPUT_PULLUP);
  pinMode(BTN_VOL_DN, INPUT_PULLUP);
  pinMode(BTN_PLAY,   INPUT_PULLUP);
  pinMode(BTN_MODE,   INPUT_PULLUP);
  pinMode(BTN_PREV,   INPUT_PULLUP);
  pinMode(BTN_NEXT,   INPUT_PULLUP);

  Wire.begin(8, 9);
  Wire.setClock(400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for (;;); }
  display.setTextWrap(false);

  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);
  if (!SD.begin(SD_CS_PIN)) {
    currentTrackName = "SD Error!";
    drawStatic();
    for (;;);
  }

  // SD Kartı Tara
  File root = SD.open("/");
  scanSDCard(root);
  root.close();

  if (playlist.empty()) {
    currentTrackName = "No MP3 Files Found!";
    drawStatic();
    for (;;);
  }

  // Şarkıları Alfabetik Olarak Sırala
  std::sort(playlist.begin(), playlist.end());

  audio.setPinout(I2S_BCK_PIN, I2S_LCK_PIN, I2S_DATA_PIN);
  audio.setVolume(volumePercent);

  // İlk şarkıyı başlat
  playTrack(currentTrackIndex);
}

void loop() {
  audio.loop(); 
  checkButtons();

  static unsigned long lastProgressUpdate = 0;
  if (millis() - lastProgressUpdate > 1000) {
    needsRedraw = true;
    lastProgressUpdate = millis();
  }

  if (needsRedraw) {
    drawStatic();
    needsRedraw = false;
  }

  updateScrollingText();
}

// --- Şarkı Bittiğinde Tetiklenen Kütüphane Fonksiyonu ---
void audio_eof_mp3(const char *info) {
  if (currentMode == REPEAT_ONE) {
    playTrack(currentTrackIndex); // Aynı şarkıyı baştan çal
  } else if (currentMode == NORMAL && currentTrackIndex == playlist.size() - 1) {
    // Normal modda son şarkı bittiyse durdur
    isPlaying = false;
    needsRedraw = true;
  } else {
    nextTrack(); // Shuffle veya Repeat All durumunda direkt sonrakine geç
  }
}

// Boş bırakılan zorunlu kütüphane fonksiyonları
void audio_info(const char *info){}
void audio_id3data(const char *info){}
