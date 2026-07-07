#include "Arduino.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <driver/i2s.h> 

// oled screen settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// buttons
#define BTN_VOL_UP   3   // Volume +
#define BTN_VOL_DN   35  // Volume -
#define BTN_PLAY     36  // Start / Stop
#define BTN_MODE     48  // (Normal / Shuffle / Repeat All / Repeat One)
#define BTN_PREV     47  // previus song
#define BTN_NEXT     21  // next song


#define ANALOG_LEFT_PIN  1 
#define ANALOG_RIGHT_PIN 2 

#define I2S_BCK_PIN  4      
#define I2S_DATA_PIN 5      
#define I2S_LCK_PIN  6     

bool isPlaying = true;
int volumePercent = 75; 
int progress = 0;
String currentTrackName = "Pulse-X Bluetooth 5.0 Audio Stream";

int scrollX = 12;
unsigned long lastScrollTime = 0;
const int scrollSpeed = 35;
const int startDelay = 2000;
bool isDelaying = true;

unsigned long lastBtnTime = 0;
const int debounce = 250; 
bool needsRedraw = true;


void init_i2s_dac() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,                           
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,   
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,   
    .communication_format = I2S_COMM_FORMAT_STAND_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,       
    .dma_buf_count = 8,                             
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

 
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}


void drawStatic() {
  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 12, SSD1306_WHITE);


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

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(8, 6);
  display.print("V:%");
  display.print(volumePercent);


  display.setCursor(98, 6);
  display.print("BT");

  display.drawRoundRect(20, 54, 88, 4, 1, SSD1306_WHITE);
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
    progress = 0; 
    needsRedraw = true;
    lastBtnTime = now;
  }
}

void setup() {

  pinMode(BTN_VOL_UP, INPUT_PULLUP);
  pinMode(BTN_VOL_DN, INPUT_PULLUP);
  pinMode(BTN_PLAY,   INPUT_PULLUP);
  pinMode(BTN_BT,     INPUT_PULLUP);
  pinMode(BTN_PREV,   INPUT_PULLUP);
  pinMode(BTN_NEXT,   INPUT_PULLUP);

  analogReadResolution(12);


  Wire.begin(8, 9);
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;); 
  }

  display.setTextWrap(false);
  init_i2s_dac(); 
  drawStatic();
}

void loop() {
  checkButtons();


  if (isPlaying) {
    int16_t leftSample = (analogRead(ANALOG_LEFT_PIN) - 2048);
    int16_t rightSample = (analogRead(ANALOG_RIGHT_PIN) - 2048);

    leftSample = (leftSample * volumePercent) / 100 * 16;
    rightSample = (rightSample * volumePercent) / 100 * 16;


    int16_t i2sBuffer[2] = {leftSample, rightSample};
    size_t bytesWritten;
    
    i2s_write(I2S_NUM_0, i2sBuffer, sizeof(i2sBuffer), &bytesWritten, portMAX_DELAY);
  }

  if (needsRedraw) {
    drawStatic();
    needsRedraw = false;
  }

  updateScrollingText();

  if (isPlaying) {
    static unsigned long lastProgress = 0;
    if (millis() - lastProgress > 1500) {
      progress = (progress + 1) % 101;
      lastProgress = millis();
      needsRedraw = true;
    }
  }
}
