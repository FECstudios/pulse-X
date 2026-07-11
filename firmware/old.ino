#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>
#include <Audio.h>

// --- OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SDA_PIN 8
#define SCL_PIN 9
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- SD Kart SPI Pinleri ---
#define SD_CS     10
#define SPI_MOSI  11
#define SPI_SCK   12
#define SPI_MISO  13

// --- I2S Pinleri ---
#define I2S_BCLK  5
#define I2S_LRC   7
#define I2S_DOUT  6

// --- Butonlar ---
#define BTN_VOL_UP 3
#define BTN_VOL_DN 41
#define BTN_PLAY   40
#define BTN_PREV   47
#define BTN_NEXT   21

Audio audio;

// --- Playlist (SD'nde bulunan dosyalar) ---
#define MAX_TRACKS 100

String playlist[MAX_TRACKS];
int totalTracks = 0;
int currentTrackIndex = 0;

void scanFolder(File dir) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry)
      break;

    if (!entry.isDirectory()) {
      String name = entry.name();
      name.toLowerCase();

      if (name.endsWith(".mp3")) {
        if (totalTracks < MAX_TRACKS) {
          playlist[totalTracks] = "/" + String(entry.name());
          Serial.println(playlist[totalTracks]);
          totalTracks++;
        }
      }
    }
    entry.close();
  }
}

bool isPlaying = true;
int volumePercent = 14; // 0-21 arasi

unsigned long lastBtnTime = 0;
const int debounce = 250; // Buton gecikme süresi (ms)

void showStatus() {
  display.clearDisplay();

  // Çerçeve
  display.drawRoundRect(0, 0, 128, 64, 6, SSD1306_WHITE);

  // Üst çizgi
  display.drawFastHLine(2, 12, 124, SSD1306_WHITE);

  // Başlık
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(5,2);
  display.print("PULSE-X");

  display.setCursor(98,2);
  display.print("SD");

  display.fillCircle(122,5,2,SSD1306_WHITE);

  // Play / Pause
  display.setCursor(34,18);

  if(isPlaying){
    display.print((char)16);   // ►
    display.print(" PLAYING");
  }else{
    display.print("|| PAUSED");
  }

  // Dosya adı
  String name = playlist[currentTrackIndex];
  name.replace("/", "");

  display.setCursor(10,34);
  display.print(name);

  // Ses
  display.setCursor(5,49);
  display.print("VOL");

  display.drawRect(30,48,50,8,SSD1306_WHITE);

  int bar=map(volumePercent,0,21,0,48);

  display.fillRect(31,49,bar,6,SSD1306_WHITE);

  display.setCursor(87,49);
  display.print(volumePercent);
  display.print("/21");

  display.display();
}

void playTrack(int index){
    audio.connecttoFS(SD, playlist[index].c_str());
    isPlaying = true;
    showStatus();
}

void checkButtons() {
  // Tanımladığınız debounce süresi dolmadıysa butonları kontrol etme
  if (millis() - lastBtnTime < debounce) return; 

  static bool upLast   = HIGH;
  static bool downLast = HIGH;
  static bool playLast = HIGH;
  static bool prevLast = HIGH;
  static bool nextLast = HIGH;

  bool changed = false;

  // Ses +
  bool up = digitalRead(BTN_VOL_UP);
  if (up == LOW && upLast == HIGH) {
    volumePercent = min(21, volumePercent + 1);
    audio.setVolume(volumePercent);
    showStatus();
    Serial.println("VOL +");
    changed = true;
  }
  upLast = up;

  // Ses -
  bool down = digitalRead(BTN_VOL_DN);
  if (down == LOW && downLast == HIGH) {
    volumePercent = max(0, volumePercent - 1);
    audio.setVolume(volumePercent);
    showStatus();
    Serial.println("VOL -");
    changed = true;
  }
  downLast = down;

  // Play / Pause
  bool play = digitalRead(BTN_PLAY);
  if (play == LOW && playLast == HIGH) {
    audio.pauseResume();
    isPlaying = !isPlaying;
    showStatus();
    Serial.println("PLAY");
    changed = true;
  }
  playLast = play;

  // Önceki
  bool prev = digitalRead(BTN_PREV);
  if (prev == LOW && prevLast == HIGH) {
    currentTrackIndex--;
    if (currentTrackIndex < 0) currentTrackIndex = totalTracks - 1; // Hata düzeltildi: totalTracks
    playTrack(currentTrackIndex);
    showStatus();
    Serial.println("PREV");
    changed = true;
  }
  prevLast = prev;

  // Sonraki
  bool next = digitalRead(BTN_NEXT);
  if (next == LOW && nextLast == HIGH) {
    currentTrackIndex++;
    if (currentTrackIndex >= totalTracks) currentTrackIndex = 0; // Hata düzeltildi: totalTracks
    playTrack(currentTrackIndex);
    showStatus();
    Serial.println("NEXT");
    changed = true;
  }
  nextLast = next;

  if (changed) {
    lastBtnTime = millis();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BTN_VOL_UP, INPUT_PULLUP);
  pinMode(BTN_VOL_DN, INPUT_PULLUP);
  pinMode(BTN_PLAY,   INPUT_PULLUP);
  pinMode(BTN_PREV,   INPUT_PULLUP);
  pinMode(BTN_NEXT,   INPUT_PULLUP);

  // OLED başlat
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED bulunamadi!");
    while (1);
  }
  display.setTextWrap(false);
  display.setRotation(2); // Ekranı 180 derece ters çevir

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("SD baslatiliyor...");
  display.display();

  // SD kart başlat
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI)) {
    Serial.println("SD FAIL");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("SD KART BULUNAMADI");
    display.display();
    while (1);
  }
  Serial.println("SD OK");
  File root = SD.open("/");
  scanFolder(root);
  root.close();

  Serial.print("Toplam MP3: ");
  Serial.println(totalTracks);

  if(totalTracks == 0){
      Serial.println("MP3 bulunamadi!");
      while(1);
  }
  
  // I2S ses çıkışı
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volumePercent);

  // İlk parçayı çal
  playTrack(currentTrackIndex);
}

void loop() {
  audio.loop();
  checkButtons();
}

void audio_info(const char *info) {
  Serial.println(info);
}

void audio_eof_mp3(const char *info) {
  Serial.println("EOF");
  currentTrackIndex = (currentTrackIndex + 1) % totalTracks; // Hata düzeltildi: totalTracks
  playTrack(currentTrackIndex);
}
