#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>
#include <Audio.h>

// Pin Definitions
#define SDA_PIN     8
#define SCL_PIN     9
#define SD_CS       10
#define SPI_MOSI    11
#define SPI_SCK     12
#define SPI_MISO    13
#define I2S_BCLK    5
#define I2S_LRC     7
#define I2S_DOUT    6
#define BTN_VOL_UP  3
#define BTN_VOL_DN  41
#define BTN_PLAY    40
#define BTN_PREV    47
#define BTN_NEXT    21

// Display Configurations
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define I2C_ADDRESS   0x3C

// Audio Configurations
#define MAX_TRACKS    100
#define MAX_VOLUME    21
#define DEBOUNCE_MS   250
#define SCROLL_MS     40

// Global Objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Audio audio;

// System States
String playlist[MAX_TRACKS];
int totalTracks = 0;
int currentTrackIndex = 0;
bool isPlaying = true;
bool pendingEof = false;
bool startupPlayed = false;
bool sysSoundPlaying = false;
int volumePercent = 10;
unsigned long lastBtnTime = 0;

// Scrolling Text Variables
int scrollX = SCREEN_WIDTH;
unsigned long lastScrollTime = 0;
String scrollName = "";

/**
 * Checks if the given file name belongs to system notification sounds.
 */
bool isSysFile(const char* name) {
  return (strcmp(name, "0001.mp3") == 0 || strcmp(name, "0002.mp3") == 0 ||
          strcmp(name, "0003.mp3") == 0 || strcmp(name, "0004.mp3") == 0);
}

/**
 * Scans the root directory for valid MP3 files and populates the playlist.
 */
void scanFolder(File dir) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;

    if (!entry.isDirectory()) {
      const char* name = entry.name();
      String lowName = String(name);
      lowName.toLowerCase();

      if (lowName.endsWith(".mp3") && !isSysFile(name) && totalTracks < MAX_TRACKS) {
        playlist[totalTracks++] = "/" + String(name);
      }
    }
    entry.close();
  }
}

/**
 * Resets the scrolling properties for the currently active track.
 */
void resetScroll() {
  scrollX = 20; // Set to 20 for immediate visibility on screen
  if (totalTracks > 0) {
    scrollName = playlist[currentTrackIndex];
    scrollName.replace("/", "");
    if (scrollName.endsWith(".mp3") || scrollName.endsWith(".MP3")) {
      scrollName = scrollName.substring(0, scrollName.length() - 4);
    }
  }
}

/**
 * Updates and renders the main playback status interface.
 */
void showStatus() {
  display.clearDisplay();
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 5, SSD1306_WHITE);
  display.drawFastHLine(2, 13, 124, SSD1306_WHITE);
  display.drawFastHLine(2, 50, 124, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Header Info
  display.setCursor(5, 3);
  display.print("PULSE-X");
  display.setCursor(88, 3);
  display.print(currentTrackIndex + 1);
  display.print("/");
  display.print(totalTracks);

  // Playback Icon State
  display.setCursor(5, 16);
  if (isPlaying) {
    display.print((char)16); // Play icon
  } else {
    display.print("||");       // Pause icon
  }

  // Scrolling Track Name
  display.setCursor(scrollX, 28);
  display.print(scrollName);

  // Volume Info & Bar
  int volPct = map(volumePercent, 0, MAX_VOLUME, 0, 100);
  display.setCursor(5, 54);
  display.print("VOL ");
  display.print(volPct);
  display.print("%");

  int barW = map(volumePercent, 0, MAX_VOLUME, 0, 60);
  display.drawRect(62, 53, 62, 8, SSD1306_WHITE);
  display.fillRect(63, 54, barW, 6, SSD1306_WHITE);

  display.display();
}

/**
 * Displays a generic info or system message on screen.
 */
void showMsg(const char* msg) {
  display.clearDisplay();
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 5, SSD1306_WHITE);
  display.setTextSize(1); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 4); 
  display.print("PULSE-X");
  display.drawFastHLine(2, 14, 124, SSD1306_WHITE);
  display.setCursor(5, 22); 
  display.print(msg);
  display.display();
}

/**
 * Connects and starts playing the track at the specified index.
 */
void playTrack(int index) {
  if (totalTracks == 0) return;
  audio.connecttoFS(SD, playlist[index].c_str());
  isPlaying = true;
  resetScroll();
  showStatus();
}

/**
 * Plays a system sound effect without updating track data.
 */
void playSys(const char* path) {
  if (SD.exists(path)) { 
    audio.connecttoFS(SD, path); 
    sysSoundPlaying = true; 
  }
}

/**
 * Handles text scrolling animation for long track titles.
 */
void updateScroll() {
  if (!isPlaying) return;
  if (millis() - lastScrollTime < SCROLL_MS) return;
  
  lastScrollTime = millis();
  int textW = scrollName.length() * 6; // Approx width in standard text size
  scrollX -= 2;
  
  if (scrollX < -(textW)) {
    scrollX = SCREEN_WIDTH;
  }
  showStatus();
}

/**
 * Non-blocking button handler with software debounce.
 */
void checkButtons() {
  if (millis() - lastBtnTime < DEBOUNCE_MS) return;
  
  static bool upL = HIGH, dnL = HIGH, plL = HIGH, prL = HIGH, nxL = HIGH;
  bool changed = false;

  // Volume Up
  bool up = digitalRead(BTN_VOL_UP);
  if (up == LOW && upL == HIGH) {
    volumePercent = min(MAX_VOLUME, volumePercent + 1);
    audio.setVolume(volumePercent);
    if (!sysSoundPlaying) playSys("/0003.mp3");
    showStatus(); 
    changed = true;
  } 
  upL = up;

  // Volume Down
  bool dn = digitalRead(BTN_VOL_DN);
  if (dn == LOW && dnL == HIGH) {
    volumePercent = max(0, volumePercent - 1);
    audio.setVolume(volumePercent);
    if (!sysSoundPlaying) playSys("/0004.mp3");
    showStatus(); 
    changed = true;
  } 
  dnL = dn;

  // Play / Pause
  bool pl = digitalRead(BTN_PLAY);
  if (pl == LOW && plL == HIGH) {
    audio.pauseResume(); 
    isPlaying = !isPlaying;
    showStatus(); 
    changed = true;
  } 
  plL = pl;

  // Prevent divide by zero if playlist is empty for track hopping
  if (totalTracks > 0) {
    // Previous Track
    bool pr = digitalRead(BTN_PREV);
    if (pr == LOW && prL == HIGH) {
      currentTrackIndex = (currentTrackIndex - 1 + totalTracks) % totalTracks;
      playTrack(currentTrackIndex); 
      changed = true;
    } 
    prL = pr;

    // Next Track
    bool nx = digitalRead(BTN_NEXT);
    if (nx == LOW && nxL == HIGH) {
      currentTrackIndex = (currentTrackIndex + 1) % totalTracks;
      playTrack(currentTrackIndex); 
      changed = true;
    } 
    nxL = nx;
  }

  if (changed) {
    lastBtnTime = millis();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  // Initialize Buttons
  pinMode(BTN_VOL_UP, INPUT_PULLUP); 
  pinMode(BTN_VOL_DN, INPUT_PULLUP);
  pinMode(BTN_PLAY,   INPUT_PULLUP);   
  pinMode(BTN_PREV,   INPUT_PULLUP);
  pinMode(BTN_NEXT,   INPUT_PULLUP);

  // Initialize I2C Display
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDRESS)) {
    while (1); // Halt if OLED fails
  }
  display.setTextWrap(false);
  display.setRotation(2);
  showMsg("Initializing SD...");

  // Initialize SPI & SD Card
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);
  delay(100);
  if (!SD.begin(SD_CS, SPI)) { 
    showMsg("NO SD CARD!"); 
    while (1); 
  }

  // Parse Audio Tracks
  File root = SD.open("/");
  scanFolder(root); 
  root.close();
  
  Serial.print("Tracks discovered: "); 
  Serial.println(totalTracks);
  
  if (totalTracks == 0) { 
    showMsg("No MP3 found!"); 
    while (1); 
  }

  // Audio Setup
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volumePercent);

  resetScroll();
  showMsg("Welcome!");
  playSys("/0001.mp3");
}

void loop() {
  audio.loop();
  checkButtons();
  
  if (pendingEof) { 
    pendingEof = false; 
    playTrack(currentTrackIndex); 
  }
  
  if (startupPlayed && !sysSoundPlaying) {
    updateScroll();
  }
}

// Audio Library Callbacks
void audio_info(const char* info) { 
  Serial.println(info); 
}

void audio_eof_mp3(const char* info) {
  if (!startupPlayed) { 
    startupPlayed = true; 
    sysSoundPlaying = false; 
    playTrack(0); 
    return; 
  }
  if (sysSoundPlaying) { 
    sysSoundPlaying = false; 
    pendingEof = true; 
    return; 
  }
  
  currentTrackIndex = (currentTrackIndex + 1) % totalTracks;
  pendingEof = true;
}
