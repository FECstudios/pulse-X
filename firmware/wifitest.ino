#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>
#include <Audio.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include "SoapESP32.h"

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

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define MAX_TRACKS    100
#define MAX_DLNA      100
#define MAX_VOLUME    21
#define DEBOUNCE_MS   250
#define SCROLL_MS     40

#define WIFI_SSID "MSCCruse3"
#define WIFI_PASS "erayeray"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Audio audio;
WiFiClient client;
WiFiUDP udp;
SoapESP32 soap(&client, &udp);

// --- SD ---
String playlist[MAX_TRACKS];
int totalTracks = 0;
int currentTrackIndex = 0;

// --- UPnP ---
struct DlnaTrack { String name; String url; };
DlnaTrack dlnaList[MAX_DLNA];
int dlnaTotal = 0;
int dlnaIndex = 0;

// --- State ---
bool isPlaying = true;
bool pendingEof = false;
bool startupPlayed = false;
bool sysSoundPlaying = false;
bool upnpMode = false;
bool pendingDlnaNext = false;
int volumePercent = 10;
unsigned long lastBtnTime = 0;

// --- Scroll ---
int scrollX = SCREEN_WIDTH;
unsigned long lastScrollTime = 0;
String scrollName = "";

bool isSysFile(const char* name) {
  return (strcmp(name,"0001.mp3")==0 || strcmp(name,"0002.mp3")==0 ||
          strcmp(name,"0003.mp3")==0 || strcmp(name,"0004.mp3")==0);
}

void scanFolder(File dir) {
  while (true) {
    File e = dir.openNextFile();
    if (!e) break;
    if (!e.isDirectory()) {
      const char* n = e.name();
      String low = String(n); low.toLowerCase();
      if (low.endsWith(".mp3") && !isSysFile(n) && totalTracks < MAX_TRACKS)
        playlist[totalTracks++] = "/" + String(n);
    }
    e.close();
  }
}

void resetScroll() {
  scrollX = 20;
  if (upnpMode && dlnaTotal > 0) {
    scrollName = dlnaList[dlnaIndex].name;
    if (scrollName.endsWith(".mp3") || scrollName.endsWith(".MP3"))
      scrollName = scrollName.substring(0, scrollName.length()-4);
  } else if (!upnpMode && totalTracks > 0) {
    scrollName = playlist[currentTrackIndex];
    scrollName.replace("/", "");
    if (scrollName.endsWith(".mp3") || scrollName.endsWith(".MP3"))
      scrollName = scrollName.substring(0, scrollName.length()-4);
  }
}

void showStatus() {
  display.clearDisplay();
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 5, SSD1306_WHITE);
  display.drawFastHLine(2, 13, 124, SSD1306_WHITE);
  display.drawFastHLine(2, 50, 124, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(5, 3); display.print("PULSE-X");
  display.setCursor(82, 3);
  if (upnpMode) { display.print("UPnP"); }
  else {
    display.print(currentTrackIndex+1);
    display.print("/");
    display.print(totalTracks);
  }

  display.setCursor(5, 16);
  if (isPlaying) { display.print((char)16); }
  else            { display.print("||"); }

  display.setCursor(scrollX, 28);
  display.print(scrollName);

  int volPct = map(volumePercent, 0, MAX_VOLUME, 0, 100);
  display.setCursor(5, 54);
  display.print("VOL "); display.print(volPct); display.print("%");
  int barW = map(volumePercent, 0, MAX_VOLUME, 0, 60);
  display.drawRect(62, 53, 62, 8, SSD1306_WHITE);
  display.fillRect(63, 54, barW, 6, SSD1306_WHITE);
  display.display();
}

void showMsg(const char* msg) {
  display.clearDisplay();
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 5, SSD1306_WHITE);
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 4); display.print("PULSE-X");
  display.drawFastHLine(2, 14, 124, SSD1306_WHITE);
  display.setCursor(5, 22); display.print(msg);
  display.display();
}

void playTrack(int index) {
  if (totalTracks == 0) return;
  audio.connecttoFS(SD, playlist[index].c_str());
  isPlaying = true;
  resetScroll();
  showStatus();
}

void playDlna(int index) {
  if (dlnaTotal == 0) return;

  Serial.println("====================");
  Serial.println(dlnaList[index].name);
  Serial.println(dlnaList[index].url);

  bool ok = audio.connecttohost(dlnaList[index].url.c_str());

  Serial.print("connecttohost = ");
  Serial.println(ok);

  isPlaying = true;
  resetScroll();
  showStatus();
}

void playSys(const char* path) {
  if (SD.exists(path)) { audio.connecttoFS(SD, path); sysSoundPlaying=true; }
}

String dlnaBase = ""; // global — diğer globallerle beraber

void browseInto(int srvIdx, const char* folderId) {
  if (dlnaTotal >= MAX_DLNA) return;
  soapObjectVect_t items;
  if (!soap.browseServer(srvIdx, folderId, &items)) return;
  for (auto& item : items) {
    if (!item.isDirectory) {
      if (item.uri.length() > 5 && dlnaTotal < MAX_DLNA) {
        dlnaList[dlnaTotal].name = item.name;
        dlnaList[dlnaTotal].url = item.uri.startsWith("http") ? item.uri : dlnaBase + item.uri;
        Serial.println(dlnaList[dlnaTotal].url);
        dlnaTotal++;
      }
    }
  }
}

bool scanDLNA() {
  dlnaTotal = 0;
  dlnaBase = "";
  showMsg("WiFi baglaniyor...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) { delay(500); tries++; }
  if (WiFi.status() != WL_CONNECTED) { showMsg("WiFi basarisiz!"); delay(1000); return false; }

  showMsg("DLNA taranıyor...");
  if (!soap.seekServer()) { showMsg("Server yok!"); delay(1000); return false; }

  int srvIdx = -1;
  soapServer_t srv;
  for (int i = 0; i < soap.getServerCount(); i++) {
    if (soap.getServerInfo(i, &srv)) {
      Serial.println(srv.friendlyName);
      if (srv.friendlyName.indexOf("Bubble") >= 0 || srv.friendlyName.indexOf("Media") >= 0) {
        srvIdx = i; break;
      }
    }
  }
  if (srvIdx == -1) srvIdx = 0;
  soap.getServerInfo(srvIdx, &srv);
  dlnaBase = "http://" + srv.ip.toString() + ":" + String(srv.port) + "/";
  Serial.print("Base: "); Serial.println(dlnaBase);

  soapObjectVect_t root;
  soap.browseServer(srvIdx, "/external/audio/media", &root);
  for (auto& item : root) {
    if (!item.isDirectory) continue;
    Serial.print("Deneniyor: "); Serial.println(item.name);
    if (item.name == "All tracks") {
      browseInto(srvIdx, item.id.c_str());
      break;
    }
  }

  if (dlnaTotal == 0) browseInto(srvIdx, "/external/audio/media");

  Serial.print("Toplam DLNA: "); Serial.println(dlnaTotal);
  if (dlnaTotal == 0) { showMsg("Sarki bulunamadi!"); delay(1000); return false; }
  return true;
}


void switchToUPnP() {
  upnpMode = true;
  audio.stopSong();
  if (scanDLNA()) {
    dlnaIndex = 0;
    playDlna(dlnaIndex);
  } else {
    upnpMode = false;
    WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
    playTrack(currentTrackIndex);
  }
}

void switchToSD() {
  upnpMode = false;
  audio.stopSong();
  WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
  dlnaTotal = 0;
  playTrack(currentTrackIndex);
}

void updateScroll() {
  if (!isPlaying) return;
  if (millis() - lastScrollTime < SCROLL_MS) return;
  lastScrollTime = millis();
  int textW = scrollName.length() * 6;
  scrollX -= 2;
  if (scrollX < -(textW)) scrollX = SCREEN_WIDTH;
  showStatus();
}

void checkButtons() {
  if (millis() - lastBtnTime < DEBOUNCE_MS) return;
  static bool upL=HIGH,dnL=HIGH,plL=HIGH,prL=HIGH,nxL=HIGH;
  bool changed=false;

  bool up=digitalRead(BTN_VOL_UP);
  bool dn=digitalRead(BTN_VOL_DN);
  bool pl=digitalRead(BTN_PLAY);
  bool pr=digitalRead(BTN_PREV);
  bool nx=digitalRead(BTN_NEXT);

  if (pr==LOW && nx==LOW) {
    if (upnpMode) switchToSD();
    else switchToUPnP();
    delay(600); lastBtnTime=millis(); return;
  }

  if (up==LOW && upL==HIGH) {
    volumePercent=min(MAX_VOLUME,volumePercent+1);
    audio.setVolume(volumePercent);
    if (!sysSoundPlaying && !upnpMode) playSys("/0003.mp3");
    showStatus(); changed=true;
  } upL=up;

  if (dn==LOW && dnL==HIGH) {
    volumePercent=max(0,volumePercent-1);
    audio.setVolume(volumePercent);
    if (!sysSoundPlaying && !upnpMode) playSys("/0004.mp3");
    showStatus(); changed=true;
  } dnL=dn;

  if (pl==LOW && plL==HIGH) {
    if (upnpMode) {
      dlnaIndex=(dlnaIndex+1)%dlnaTotal;
      playDlna(dlnaIndex);
    } else {
      audio.pauseResume(); isPlaying=!isPlaying;
    }
    showStatus(); changed=true;
  } plL=pl;

  if (pr==LOW && prL==HIGH) {
    if (upnpMode && dlnaTotal>0) {
      dlnaIndex=(dlnaIndex-1+dlnaTotal)%dlnaTotal;
      playDlna(dlnaIndex);
    } else if (totalTracks>0) {
      currentTrackIndex=(currentTrackIndex-1+totalTracks)%totalTracks;
      playTrack(currentTrackIndex);
    }
    changed=true;
  } prL=pr;

  if (nx==LOW && nxL==HIGH) {
    if (upnpMode && dlnaTotal>0) {
      dlnaIndex=(dlnaIndex+1)%dlnaTotal;
      playDlna(dlnaIndex);
    } else if (totalTracks>0) {
      currentTrackIndex=(currentTrackIndex+1)%totalTracks;
      playTrack(currentTrackIndex);
    }
    changed=true;
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
  showMsg("SD baslatiliyor...");

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);
  delay(100);
  if (!SD.begin(SD_CS, SPI)) { showMsg("SD KART YOK!"); while(1); }

  File root = SD.open("/");
  scanFolder(root); root.close();
  if (totalTracks==0) { showMsg("MP3 bulunamadi!"); while(1); }

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volumePercent);

  resetScroll();
  showMsg("Hosgeldin!");
  playSys("/0001.mp3");
}

void loop() {
  audio.loop();
  checkButtons();
  if (pendingEof) { pendingEof=false; playTrack(currentTrackIndex); }
  if (pendingDlnaNext) {
    pendingDlnaNext=false;
    dlnaIndex=(dlnaIndex+1)%dlnaTotal;
    playDlna(dlnaIndex);
  }
  if (startupPlayed && !sysSoundPlaying) updateScroll();
}

void audio_info(const char* info) { 
  Serial.println(info); 
  
  }
void audio_showstreamtitle(const char *info)
{
    Serial.print("TITLE: ");
    Serial.println(info);
}

void audio_bitrate(const char *info)
{
    Serial.print("BITRATE: ");
    Serial.println(info);
}

void audio_id3data(const char *info)
{
    Serial.print("ID3: ");
    Serial.println(info);
}
void audio_eof_mp3(const char* info) {
  if (!startupPlayed) { startupPlayed=true; sysSoundPlaying=false; playTrack(0); return; }
  if (sysSoundPlaying) { sysSoundPlaying=false; pendingEof=true; return; }
  if (upnpMode) { pendingDlnaNext=true; }
  else { currentTrackIndex=(currentTrackIndex+1)%totalTracks; pendingEof=true; }
}
