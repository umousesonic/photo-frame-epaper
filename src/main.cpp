/* WiFi and HTTPS */
#include <Arduino.h>
#include <Wire.h>
#include "esp_jpg_decode.h"
#include <esp_task_wdt.h>
#include <WiFi.h>
#include "WiFiManager/WiFiManager.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include "main.h"
#include "epd/epd.hpp"
#include "JPEGDEC.h"
#include "RtcDS3231.h"


/* JPEG decoder*/
JPEGDEC jpeg;

/* WiFi client*/
WiFiClientSecure *client = new WiFiClientSecure;

/* WiFi manager */
WiFiManagerParameter parameter("gflink", "Link to Google album", "", 65535);
Preferences preferences;

/* RTC */
RtcDS3231<TwoWire> Rtc(Wire);

/* HTTP */
const char *headerkeys[] = {"Location"};

/* Google photo */
char photoIdList[PHOTO_LIMIT][PHOTO_ID_SIZE];
uint8_t *photoBuf;
uint8_t *dispBuf;
int16_t* errors;

/* variables */
static int len, offset, photoCount;
static bool shownPhoto = false;

String GOOGLE_PHOTO_SHARE_LINK = "";

uint8_t find_closest_palette_index(Pixel_t pixel) {
  uint8_t closest = 0;
  int32_t min_error = INT32_MAX;
  for (int i = 0; i < 7; i ++) {
    int error_r = pixel.r - color_palette[i].r;
    if (error_r < 0) error_r = -error_r;
    int error_g = pixel.g - color_palette[i].g;
    if (error_g < 0) error_g = -error_g;
    int error_b = pixel.b - color_palette[i].b;
    if (error_b < 0) error_b = -error_b;
    int error = error_r + error_g + error_b;
    if (error < min_error) {
      min_error = error;
      closest = i;
    }
  }
  return closest;
}

uint16_t* getPixelPtr(int x, int y) {
  return (uint16_t*) (((uint16_t*)dispBuf) + y*MYW + x);
}

Pixel_t getPixel(int x, int y) {
  uint16_t* pixel = getPixelPtr(x, y);
  return Pixel_t({(uint8_t) ((*pixel & 0xF800) >> 8),
                  (uint8_t) ((*pixel & 0x07E0) >> 3),
                  (uint8_t) ((*pixel & 0x001F) << 3)});
}

void setPixel(int x, int y, Pixel_t pixel) {
  uint16_t* pixel_ptr = getPixelPtr(x, y);
  *pixel_ptr = ((pixel.r & 0x1F) << 11) |
               ((pixel.g & 0x3F) << 5) |
               ((pixel.b & 0x1F) << 0);
}

Pixel_t find_closest_palette_color(Pixel_t pixel) {
  uint8_t index = find_closest_palette_index(pixel);
  return color_palette[index];
}

void shrink_info() {
  int offset = 0;
  for (int y = 0; y < MYH; y ++) {
    for (int x = 0; x < MYW; x ++) {
      // Draw each pixel
      if (x % 2 == 1) {
        // First Pixel
        uint16_t* pixel = getPixelPtr(x-1, y);
        Pixel_t first_pixel;
        first_pixel.r = ((*pixel) & 0b1111100000000000) >> 8;
        first_pixel.g = ((*pixel) & 0b0000011111100000) >> 3;
        first_pixel.b = ((*pixel) & 0b0000000000011111) << 3;

        // Second Pixel
        pixel = getPixelPtr(x, y);
        Pixel_t second_pixel;
        second_pixel.r = ((*pixel) & 0b1111100000000000) >> 8;
        second_pixel.g = ((*pixel) & 0b0000011111100000) >> 3;
        second_pixel.b = ((*pixel) & 0b0000000000011111) << 3;

        // Find closest color in palette
        uint8_t combined = 0;
        combined = find_closest_palette_index(first_pixel) << 4 | (find_closest_palette_index(second_pixel) & 0x0F);

        // Write to buffer
        dispBuf[((y*MYW)/2)+((x-1)/2)] = combined;
      }
      offset ++;
    }
  }
}

int drawFunc(JPEGDRAW* pdraw) {
  int offset = 0;
  for (int y = pdraw->y; y < pdraw->y + pdraw->iHeight; y ++) {
    for (int x = pdraw->x; x < pdraw->x + pdraw->iWidth; x ++) {
      // Draw each pixel
      if (x % 2 == 1) {
        // First Pixel
        uint16_t* pixel = pdraw->pPixels + offset - 1;
        Pixel_t first_pixel;
        first_pixel.r = ((*pixel) & 0b1111100000000000) >> 8;
        first_pixel.g = ((*pixel) & 0b0000011111100000) >> 3;
        first_pixel.b = ((*pixel) & 0b0000000000011111) << 3;

        // Second Pixel
        pixel = pdraw->pPixels + offset;
        Pixel_t second_pixel;
        second_pixel.r = ((*pixel) & 0b1111100000000000) >> 8;
        second_pixel.g = ((*pixel) & 0b0000011111100000) >> 3;
        second_pixel.b = ((*pixel) & 0b0000000000011111) << 3;

        // Find closest color in palette
        uint8_t combined = 0;
        combined = find_closest_palette_index(first_pixel) << 4 | (find_closest_palette_index(second_pixel) & 0x0F);

        // Write to buffer
        dispBuf[((y*MYW)/2)+((x-1)/2)] = combined;
      }
      offset ++;
    }
  }
  return 1;
}

void setup_dither() {
  // Allocate memory for errors
  errors = (int16_t*)ps_malloc(MYW * MYH * 3 * sizeof(int16_t));
  if (errors == NULL) {
    Serial.println(F("ERROR: Failed to allocate memory for errors"));
    while(1);
  }
  memset(errors, 0, MYW * MYH * 3 * sizeof(int16_t));
}

int drawFunc_dither(JPEGDRAW* pdraw) {
  uint16_t* buf = pdraw->pPixels;
  int width = pdraw->iWidth;
  int height = pdraw->iHeight;
  int offset = 0;
  for (int y = pdraw->y; y < pdraw->y + pdraw->iHeight; y ++) {
    for (int x = pdraw->x; x < pdraw->x + pdraw->iWidth; x ++) {
      // Write to buffer
      // *(getPixelPtr(x, y)) = *(pdraw->pPixels + offset);

      // uint16_t color = buf[y * width + x];
      uint16_t color = *(pdraw->pPixels + offset);

      // convert the color to RGB888 format
      Pixel_t rgb = {
        (color >> 11) << 3,
        ((color >> 5) & 0x3f) << 2,
        (color & 0x1f) << 3
      };

      // adjust the color using the error diffusion matrix
      rgb.r = max(0, min(255, rgb.r + errors[(x + y*MYW) * 3 + 0] / 16));
      rgb.g = max(0, min(255, rgb.g + errors[(x + y*MYW) * 3 + 1] / 16));
      rgb.b = max(0, min(255, rgb.b + errors[(x + y*MYW) * 3 + 2] / 16));

      // find the closest color in the palette
      uint8_t closest_color = find_closest_palette_index(rgb);

      // save the result
      // dispBuf[y * width + x] = closest_color;
      setPixel(x, y, color_palette[closest_color]);

      // propagate the error to neighboring pixels using Floyd-Steinberg dithering
      int16_t er = rgb.r - ((const Pixel_t*)(&color_palette[closest_color]))->r;
      int16_t eg = rgb.g - ((const Pixel_t*)(&color_palette[closest_color]))->g;
      int16_t eb = rgb.b - ((const Pixel_t*)(&color_palette[closest_color]))->b;

      if (x < MYW - 1) {
        errors[((x+1) + (y)*MYW) * 3 + 0] += er * 7;
        errors[((x+1) + (y)*MYW) * 3 + 1] += eg * 7;
        errors[((x+1) + (y)*MYW) * 3 + 2] += eb * 7;
      }
      if (x > 0 && y < MYH - 1) {
        errors[((x-1)+ (y+1)*MYW)*3 + 0] += er * 3;
        errors[((x-1)+ (y+1)*MYW)*3 + 1] += eg * 3;
        errors[((x-1)+ (y+1)*MYW)*3 + 2] += eb * 3;
      }
      if (y < MYH - 1) {
        errors[((x) + (y+1)*MYW) * 3 + 0] += er * 5;
        errors[((x) + (y+1)*MYW) * 3 + 1] += eg * 5;
        errors[((x) + (y+1)*MYW) * 3 + 2] += eb * 5;
      }
      if (x < width - 1 && y < height - 1) {
        errors[((x+1) + (y+1)*MYW) * 3 + 0] += er;
        errors[((x+1) + (y+1)*MYW) * 3 + 1] += eg;
        errors[((x+1) + (y+1)*MYW) * 3 + 2] += eb;
      }



      offset ++;
    }
  }
  return 1;
}

void show() {
  shrink_info();
  Serial.println(F("Displaying..."));
  epd_clear(EPD_WHITE);
  epd_display_part(dispBuf, 0, 0, MYW, MYH);
  Serial.println(F("Done."));
}

/* WiFiManager Stuff*/ 
void wmsetup() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  WiFiManager wm;
  wm.addParameter(&parameter);
  bool res = wm.autoConnect("AutoConnectAP","password"); // password protected ap;

  if(!res) { Serial.println("Failed to connect"); } 
  else {Serial.println("connected...yeey :)");}

  if (strlen(parameter.getValue()) != 0) {
    preferences.begin("appdata", false);
    preferences.putString("gflink", parameter.getValue());
    preferences.end();
  }
}

String getLink() {
  preferences.begin("appdata", true);
  String link = preferences.getString("gflink");
  preferences.end();
  return link;
}

void go_to_sleep() {
  // Set next alarm
  RtcDateTime now = Rtc.GetDateTime();
  uint32_t update_interval = UPDATE_DAY*24*60*60 + UPDATE_HOUR*60*60 + UPDATE_MINUTE*60 + UPDATE_SECOND;
  RtcDateTime alarmTime = now + update_interval; // into the future
  DS3231AlarmOne alarm1(
          alarmTime.Day(),
          alarmTime.Hour(),
          alarmTime.Minute(), 
          alarmTime.Second(),
          DS3231AlarmOneControl_HoursMinutesSecondsMatch);
  Rtc.SetAlarmOne(alarm1);

  // Go to sleep
  esp_sleep_enable_ext0_wakeup((gpio_num_t)RTC_WAKE_PIN, 0);
  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
  
}

void setup() {
  Serial.begin(115200);

  // Setup RTC
  Rtc.Begin(I2C_SDA, I2C_SCL);
  Rtc.LatchAlarmsTriggeredFlags();
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmBoth);

  // Check wake up reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Woke up from RTC");
    // Clear flag
    Rtc.LatchAlarmsTriggeredFlags();
  }
  else {
    // Pushed reset button
    Serial.println("Resetting WiFi settings");
    WiFiManager wm;
    wm.resetSettings();

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    if (!Rtc.IsDateTimeValid()) {
      if (Rtc.LastError() != 0) {
        Serial.print("RTC communications error = ");
        Serial.println(Rtc.LastError());
      } else {
        Serial.println("RTC lost confidence in the DateTime!");
        Serial.println("Setting time...");
        Rtc.SetDateTime(compiled);
      }
    }
    if (!Rtc.GetIsRunning()) {
      Serial.println("RTC was not actively running, starting now");
      Rtc.SetIsRunning(true);
    }
  }

  // init display
  int w = MYW;
  int h = MYH;

  Serial.println(F("Google Photo Frame"));
  Serial.print(F("Screen: "));
  Serial.print(w);
  Serial.print('x');
  Serial.println(h);

  // allocate photo file buffer
  photoBuf = (uint8_t *)ps_malloc(w * h);
  if (!photoBuf) {
    Serial.println(F("photoBuf allocate failed!"));
    while(1);
  }
  else {
    Serial.println(F("Using phofoBuf!!"));
  }

  dispBuf = (uint8_t *)ps_malloc(w * h * 2);
  if (!dispBuf) {
    Serial.println(F("dispBuf allocate failed!"));
    while(1);
  }
  else {
    Serial.println(F("Using dispBuf!!"));
  }
  memset (dispBuf, 0x00, w * h / 2);

  setup_dither();

  // init WiFi
  Serial.print(F("Connecting to WiFi: "));
  wmsetup();
  GOOGLE_PHOTO_SHARE_LINK = getLink();
  Serial.println(F(" done."));

  Serial.print("Album link: ");
  Serial.println(GOOGLE_PHOTO_SHARE_LINK);

  client->setCACert(rootCACertificate);

  // Init epd
  epd_init();
  delay(100);
  Serial.println("epd init done");
}

void loop() {
  HTTPClient https;

  if (!photoCount) {
    https.collectHeaders(headerkeys, sizeof(headerkeys) / sizeof(char *));
    Serial.println(GOOGLE_PHOTO_SHARE_LINK);
    Serial.println(F("[HTTPS] begin..."));
    https.begin(*client, GOOGLE_PHOTO_SHARE_LINK);
    https.setTimeout(HTTP_TIMEOUT);

    Serial.println(F("[HTTPS] GET..."));
    int httpCode = https.GET();

    Serial.print(F("[HTTPS] return code: "));
    Serial.println(httpCode);

    // redirect
    if (httpCode == HTTP_CODE_FOUND) {
      char redirectUrl[256];
      strcpy(redirectUrl, https.header((size_t)0).c_str());
      https.end();
      Serial.print(F("redirectUrl: "));
      Serial.println(redirectUrl);

      Serial.println(F("[HTTPS] begin..."));
      https.begin(*client, redirectUrl);
      https.setTimeout(HTTP_TIMEOUT);

      Serial.println(F("[HTTPS] GET..."));
      httpCode = https.GET();

      Serial.print(F("[HTTPS] GET... code: "));
      Serial.println(httpCode);
    }

    if (httpCode != HTTP_CODE_OK) {
      Serial.print(F("[HTTPS] GET... failed, error: "));
      Serial.println(https.errorToString(httpCode));
      // Go to sleep
      go_to_sleep();

    }
    else {
      // HTTP header has been send and Server response header has been handled
      //find photo ID leading pattern: ",["https://lh3.googleusercontent.com/
      photoCount = 0;
      int wait_count = 0;
      bool foundStartingPoint = false;
      WiFiClient *stream = https.getStreamPtr();
      stream->setTimeout(10);
      while ((photoCount < PHOTO_LIMIT) && (wait_count < HTTP_WAIT_COUNT)) {
        if (!stream->available()) {
          Serial.println(F("Wait stream->available()"));
          ++wait_count;
          delay(200);
        }
        else {
          if (!foundStartingPoint) {
            Serial.println(F("finding seek pattern: " SEEK_PATTERN));
            if (stream->find(SEEK_PATTERN))
            {
              Serial.println(F("found seek pattern: " SEEK_PATTERN));
              foundStartingPoint = true;
            }
          }
          else {
            if (stream->find(SEARCH_PATTERN)) {
              int i = -1;
              char c = stream->read();
              while (c != '\"') {
                photoIdList[photoCount][++i] = c;
                c = stream->read();
              }
              photoIdList[photoCount][++i] = 0; // zero tail
              Serial.println(photoIdList[photoCount]);
              ++photoCount;
            }
          }
        }
      }
      Serial.print(photoCount);
      Serial.println(F(" photo ID added."));
    }
    https.end();
  }
  else // photoCount > 0
  {
    char photoUrl[256];

    // setup url query value with LCD dimension
    int randomIdx = random(photoCount);
    char filename[16];
    sprintf(filename, "/%d.jpg", randomIdx);
    sprintf(photoUrl, PHOTO_URL_TEMPLATE, photoIdList[randomIdx], MYW, MYH);
    Serial.print(F("Random selected photo #"));
    Serial.print(randomIdx);
    Serial.print(':');
    Serial.println(photoUrl);

    Serial.println(F("[HTTP] begin..."));
    https.begin(*client, photoUrl);
    https.setTimeout(HTTP_TIMEOUT);
    Serial.println(F("[HTTP] GET..."));
    int httpCode = https.GET();

    Serial.print(F("[HTTP] GET... code: "));
    Serial.println(httpCode);
    // HTTP header has been send and Server response header has been handled
    if (httpCode != HTTP_CODE_OK) {
      Serial.print(F("[HTTP] GET... failed, error: "));
      Serial.println(https.errorToString(httpCode));
      delay(9000); // don't repeat the wrong thing too fast
    }
    else {
      // get lenght of document (is -1 when Server sends no Content-Length header)
      len = https.getSize();
      Serial.print(F("[HTTP] size: "));
      Serial.println(len);

      if (len <= 0) {
        Serial.print(F("[HTTP] Unknow content size: "));
        Serial.println(len);
      }
      else {
        // get tcp stream
        WiFiClient *photoHttpsStream = https.getStreamPtr();

        // JPG decode option 1: buffer_reader, use much more memory but faster
        size_t reads = 0;
        char buf;
        while (reads < len)
        {
          size_t r = photoHttpsStream->readBytes(photoBuf + reads, (len - reads));
          reads += r;
          Serial.print(F("Photo buffer read: "));
          Serial.print(reads);
          Serial.print('/');
          Serial.println(len);
          size_t available = photoHttpsStream->available();
        }
        if (jpeg.openRAM(photoBuf, len, drawFunc_dither)) {
          Serial.println(F("jpeg.openRAM OK"));
          jpeg.decode(0,0,0); 
        }
      }
      https.end();
    }
    show();
    shownPhoto = true;
    go_to_sleep();
  }
}
