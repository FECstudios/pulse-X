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

// --- SD ---
#define SD_CS    10
#define SPI_MOSI 11
#define SPI_SCK  12
#define SPI_MISO 13

// --- I2S ---
#define I2S_BCLK 5
#define I2S_LRC  7
#define I2S_DOUT 6

// --- Butonlar ---
#define BTN_VOL_UP 3
#define BTN_VOL_DN 41
#define BTN_PLAY   40
#define BTN_PREV   47
#define BTN_NEXT   21

// --- VHM-314 kontrol pinleri ---
// VHM-314'ün KEY pinine bağla — LOW = play/pause, pulse = next/prev
#define VHM_PLAY_PIN 15
#define VHM_NEXT_PIN 16
#define VHM_PREV_PIN 17

Audio audio;

// Sistem dosyaları — playlist'e alınmayacak
const String SYS_FILES[] = {"/0001.mp3", "/0002.mp3", "/0003.mp3", "/0004.mp3"};
#define SYS_FILE_COUNT 4

#define MAX_TRACKS 100
String playlist[MAX_TRACKS];
int totalTracks = 0;
int currentTrackIndex = 0;

bool isPlaying = true;
int volumePercent = 14;
bool btMode = false;       // false = SD, true = BT (VHM-314)
bool pendingEof = false;

unsigned long lastBtnTime = 0;
const int debounce = 250;

// --- VHM-314 sinyal gönder ---
void vhmPulse(int pin, int ms = 200) {
  digitalWrite(pin, LOW);
  delay(ms);
  digitalWrite(pin, HIGH);
}

bool isSysFile(const String& path) {
  for (int i = 0; i < SYS_FILE_COUNT; i++) {
    if (path.equalsIgnoreCase(SYS_FILES[i])) return true;
  }
  return false;
}

void scanFolder(File dir) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      String name = "/" + String(entry.name());
      String nameLow = name;
      nameLow.toLowerCase();
      if (nameLow.endsWith(".mp3") && !isSysFile(nameLow) && totalTracks < MAX_TRACKS) {
        playlist[totalTracks++] = name;
      }
    }
    entry.close();
  }
}

// --- Startup sesi çal, bitince asıl parçayı başlat ---
bool startupPlayed = false;

void playSysSound(const char* path) {
  if (SD.exists(path)) {
    audio.connecttoFS(SD, path);
  }
}

// --- OLED ---
void showStatus() {
  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 6, SSD1306_WHITE);
  display.drawFastHLine(2, 12, 124, SSD1306_WHITE);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 2);
  display.print("PULSE-X");

  // Mod göstergesi
  display.setCursor(85, 2);
  if (btMode) {
    display.print("\xF0 BT"); // BT simgesi yerine "BT"
  } else {
    display.print("SD \x10");
  }

  // Durum
  display.setCursor(5, 16);
  if (btMode) {
    display.print("  BLUETOOTH MODE");
    display.setCursor(5, 30);
    display.print("   VHM-314 aktif");
    display.setCursor(5, 44);
    display.print("Telefon baglayabilirsin");
  } else {
    // Play/Pause
    display.setCursor(34, 16);
    if (isPlaying) {
      display.print("\x10 PLAYING");
    } else {
      display.print("|| PAUSED ");
    }

    // Dosya adı — uzunsa kırp
    String name = playlist[currentTrackIndex];
    name.replace("/", "");
    if (name.length() > 16) name = name.substring(0, 15) + "~";
    display.setCursor(5, 30);
    display.print(name);

    // Track no
    display.setCursor(5, 43);
    display.print(currentTrackIndex + 1);
    display.print("/");
    display.print(totalTracks);

    // Volume bar
    display.setCursor(50, 43);
    display.print("VOL:");
    display.drawRect(80, 42, 42, 8, SSD1306_WHITE);
    int bar = map(volumePercent, 0, 21, 0, 40);
    display.fillRect(81, 43, bar, 6, SSD1306_WHITE);
  }

  display.display();
}

void showBoot(const char* msg) {
  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 6, SSD1306_WHITE);
  display.setCursor(5, 5);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.print("PULSE-X");
  display.drawFastHLine(2, 14, 124, SSD1306_WHITE);
  display.setCursor(5, 20);
  display.print(msg);
  display.display();
}

void playTrack(int index) {
  audio.connecttoFS(SD, playlist[index].c_str());
  isPlaying = true;
  showStatus();
}

void checkButtons() {
  if (millis() - lastBtnTime < debounce) return;

  static bool upL=HIGH, dnL=HIGH, plL=HIGH, prL=HIGH, nxL=HIGH;
  bool changed = false;

  bool up = digitalRead(BTN_VOL_UP);
  if (up == LOW && upL == HIGH) {
    volumePercent = min(21, volumePercent + 1);
    if (!btMode) {
      audio.setVolume(volumePercent);
      playSysSound("/0003.mp3"); // vol+ sesi
    }
    showStatus();
    changed = true;
  }
  upL = up;

  bool dn = digitalRead(BTN_VOL_DN);
  if (dn == LOW && dnL == HIGH) {
    volumePercent = max(0, volumePercent - 1);
    if (!btMode) {
      audio.setVolume(volumePercent);
      playSysSound("/0004.mp3"); // vol- sesi
    }
    showStatus();
    changed = true;
  }
  dnL = dn;

  bool pl = digitalRead(BTN_PLAY);
  if (pl == LOW && plL == HIGH) {
    if (btMode) {
      vhmPulse(VHM_PLAY_PIN); // VHM-314'e play/pause
    } else {
      audio.pauseResume();
      isPlaying = !isPlaying;
    }
    showStatus();
    changed = true;
  }
  plL = pl;

  bool pr = digitalRead(BTN_PREV);
  if (pr == LOW && prL == HIGH) {
    if (btMode) {
      vhmPulse(VHM_PREV_PIN);
    } else {
      currentTrackIndex = (currentTrackIndex - 1 + totalTracks) % totalTracks;
      playTrack(currentTrackIndex);
    }
    showStatus();
    changed = true;
  }
  prL = pr;

  bool nx = digitalRead(BTN_NEXT);
  if (nx == LOW && nxL == HIGH) {
    if (btMode) {
      vhmPulse(VHM_NEXT_PIN);
    } else {
      currentTrackIndex = (currentTrackIndex + 1) % totalTracks;
      playTrack(currentTrackIndex);
    }
    showStatus();
    changed = true;
  }
  nxL = nx;

  // PREV + NEXT aynı anda = mod değiştir
  if (pr == LOW && nx == LOW) {
    btMode = !btMode;
    if (btMode) {
      audio.stopSong();
    } else {
      audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
      audio.setVolume(volumePercent);
      playTrack(currentTrackIndex);
    }
    showStatus();
    delay(500);
    lastBtnTime = millis();
    return;
  }

  if (changed) lastBtnTime = millis();
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(BTN_VOL_UP, INPUT_PULLUP);
  pinMode(BTN_VOL_DN, INPUT_PULLUP);
  pinMode(BTN_PLAY,   INPUT_PULLUP);
  pinMode(BTN_PREV,   INPUT_PULLUP);
  pinMode(BTN_NEXT,   INPUT_PULLUP);

  pinMode(VHM_PLAY_PIN, OUTPUT); digitalWrite(VHM_PLAY_PIN, HIGH);
  pinMode(VHM_NEXT_PIN, OUTPUT); digitalWrite(VHM_NEXT_PIN, HIGH);
  pinMode(VHM_PREV_PIN, OUTPUT); digitalWrite(VHM_PREV_PIN, HIGH);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) while (1);
  display.setTextWrap(false);
  display.setRotation(2);

  showBoot("SD baslatiliyor...");

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI)) {
    showBoot("SD KART YOK!");
    while (1);
  }

  File root = SD.open("/");
  scanFolder(root);
  root.close();

  if (totalTracks == 0) {
    showBoot("MP3 bulunamadi!");
    while (1);
  }

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volumePercent);

  // Startup sesi çal
  showBoot("Hosgeldin!");
  playSysSound("/0001.mp3");
  // audio_eof_mp3 callback'i ilk parçayı başlatacak
}

void loop() {
  audio.loop();
  checkButtons();

  if (pendingEof && !btMode) {
    pendingEof = false;
    playTrack(currentTrackIndex);
  }
}

void audio_info(const char* info) { Serial.println(info); }

void audio_eof_mp3(const char* info) {
  if (!startupPlayed) {
    startupPlayed = true;
    playTrack(0); // startup bitti, ilk asıl parçayı başlat
    return;
  }
  if (!btMode) {
    currentTrackIndex = (currentTrackIndex + 1) % totalTracks;
    pendingEof = true; // loop'ta çal, callback içinde connecttoFS çağırma
  }
}
