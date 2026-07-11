#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>
#include <Audio.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SDA_PIN 8
#define SCL_PIN 9
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define SD_CS    10
#define SPI_MOSI 11
#define SPI_SCK  12
#define SPI_MISO 13

#define I2S_BCLK 5
#define I2S_LRC  7
#define I2S_DOUT 6

#define BTN_VOL_UP 3
#define BTN_VOL_DN 41
#define BTN_PLAY   40
#define BTN_PREV   47
#define BTN_NEXT   21

Audio audio;

#define MAX_TRACKS 100
String playlist[MAX_TRACKS];
int totalTracks = 0;
int currentTrackIndex = 0;
bool isPlaying = true;
int volumePercent = 2;
bool pendingEof = false;
bool startupPlayed = false;
bool sysSoundPlaying = false; // vol+/- sesi çalıyor, track advance etme

// Sistem dosyaları — playlist'e girmiyor
// 0001 = startup, 0002 = rezerve, 0003 = vol+, 0004 = vol-
const char* SYS[] = {"/0001.mp3","/0002.mp3","/0003.mp3","/0004.mp3"};

bool isSysFile(const String& p) {
  for (int i = 0; i < 4; i++) {
    String s = SYS[i]; s.toLowerCase();
    String lp = p; lp.toLowerCase();
    if (lp == s) return true;
  }
  return false;
}

void scanFolder(File dir) {
  while (true) {
    File e = dir.openNextFile();
    if (!e) break;
    if (!e.isDirectory()) {
      String name = "/" + String(e.name());
      String low = name; low.toLowerCase();
      if (low.endsWith(".mp3") && !isSysFile(low) && totalTracks < MAX_TRACKS)
        playlist[totalTracks++] = name;
    }
    e.close();
  }
}

unsigned long lastBtnTime = 0;
const int debounce = 250;

void showStatus() {
  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 6, SSD1306_WHITE);
  display.drawFastHLine(2, 12, 124, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(5, 2);  display.print("PULSE-X");
  display.setCursor(98, 2); display.print("SD");
  display.fillCircle(122, 5, 2, SSD1306_WHITE);

  display.setCursor(34, 18);
  if (isPlaying) { display.print((char)16); display.print(" PLAYING"); }
  else            { display.print("|| PAUSED"); }

  String name = playlist[currentTrackIndex];
  name.replace("/", "");
  if (name.length() > 16) name = name.substring(0, 15) + "~";
  display.setCursor(10, 34); display.print(name);

  display.setCursor(5, 49);  display.print("VOL");
  display.drawRect(30, 48, 50, 8, SSD1306_WHITE);
  display.fillRect(31, 49, map(volumePercent, 0, 21, 0, 48), 6, SSD1306_WHITE);
  display.setCursor(87, 49); display.print(volumePercent); display.print("/21");

  display.display();
}

void playTrack(int index) {
  audio.connecttoFS(SD, playlist[index].c_str());
  isPlaying = true;
  showStatus();
}

void checkButtons() {
  if (millis() - lastBtnTime < debounce) return;
  static bool upL=HIGH,dnL=HIGH,plL=HIGH,prL=HIGH,nxL=HIGH;
  bool changed = false;

  bool up = digitalRead(BTN_VOL_UP);
  if (up==LOW && upL==HIGH) {
    volumePercent = min(21, volumePercent+1);
    audio.setVolume(volumePercent);
    if (SD.exists("/0003.mp3")) { audio.connecttoFS(SD, "/0003.mp3"); sysSoundPlaying=true; }
    showStatus(); changed=true;
  } upL=up;

  bool dn = digitalRead(BTN_VOL_DN);
  if (dn==LOW && dnL==HIGH) {
    volumePercent = max(0, volumePercent-1);
    audio.setVolume(volumePercent);
    if (SD.exists("/0004.mp3")) { audio.connecttoFS(SD, "/0004.mp3"); sysSoundPlaying=true; }
    showStatus(); changed=true;
  } dnL=dn;

  bool pl = digitalRead(BTN_PLAY);
  if (pl==LOW && plL==HIGH) {
    audio.pauseResume(); isPlaying=!isPlaying;
    showStatus(); changed=true;
  } plL=pl;

  bool pr = digitalRead(BTN_PREV);
  if (pr==LOW && prL==HIGH) {
    currentTrackIndex = (currentTrackIndex-1+totalTracks)%totalTracks;
    playTrack(currentTrackIndex); changed=true;
  } prL=pr;

  bool nx = digitalRead(BTN_NEXT);
  if (nx==LOW && nxL==HIGH) {
    currentTrackIndex = (currentTrackIndex+1)%totalTracks;
    playTrack(currentTrackIndex); changed=true;
  } nxL=nx;

  if (changed) lastBtnTime=millis();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(BTN_VOL_UP,INPUT_PULLUP); pinMode(BTN_VOL_DN,INPUT_PULLUP);
  pinMode(BTN_PLAY,INPUT_PULLUP);   pinMode(BTN_PREV,INPUT_PULLUP);
  pinMode(BTN_NEXT,INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) while(1);
  display.setTextWrap(false);
  display.setRotation(2);

  display.clearDisplay();
  display.setCursor(0,0); display.println("SD baslatiliyor...");
  display.display();

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI)) {
    display.clearDisplay(); display.setCursor(0,0);
    display.println("SD KART BULUNAMADI"); display.display();
    while(1);
  }

  File root = SD.open("/");
  scanFolder(root); root.close();

  if (totalTracks==0) { Serial.println("MP3 yok!"); while(1); }

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volumePercent);

  // Startup sesi — bitince ilk parça başlayacak (audio_eof_mp3)
  if (SD.exists("/0001.mp3")) audio.connecttoFS(SD, "/0001.mp3");
  else playTrack(0);
}

void loop() {
  audio.loop();
  checkButtons();
  if (pendingEof) { pendingEof=false; playTrack(currentTrackIndex); }
}

void audio_info(const char* info) { Serial.println(info); }

void audio_eof_mp3(const char* info) {
  if (!startupPlayed) { startupPlayed=true; playTrack(0); return; }
  if (sysSoundPlaying) {
    // vol+/- sesi bitti, asıl parçaya geri dön
    sysSoundPlaying=false; pendingEof=true; return;
  }
  currentTrackIndex = (currentTrackIndex+1)%totalTracks;
  pendingEof = true;
}
