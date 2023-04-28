#ifndef TEST_DISPLAY

/* WiFi settings */
#define SSID_NAME "pwn3d"
#define SSID_PASSWORD "PageFaultError"
/* Google Photo settings */
// replace your Google Photo share link
#define GOOGLE_PHOTO_SHARE_LINK "https://photos.app.goo.gl/D9BxSJNtwNyTdr8q7"
#define PHOTO_URL_PREFIX "https://lh3.googleusercontent.com/"
#define SEEK_PATTERN "id=\"_ij\""
#define SEARCH_PATTERN "\",[\"" PHOTO_URL_PREFIX
#define PHOTO_LIMIT 10                                     // read first 10 photos to the list, ESP32 can add more
#define PHOTO_ID_SIZE 159                                  // the photo ID should be 158 charaters long and then add a zero-tail
#define HTTP_TIMEOUT 60000                                 // in ms, wait a while for server processing
#define HTTP_WAIT_COUNT 10                                 // number of times wait for next HTTP packet trunk
#define PHOTO_URL_TEMPLATE PHOTO_URL_PREFIX "%s=w%d-h%d-c" // photo id, display width and height

#define MYW 600
#define MYH 448

/* WiFi and HTTPS */
#include <Arduino.h>
#include "esp_jpg_decode.h"
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "main.h"
#include "epd/epd.hpp"
#include "JPEGDEC.h"
// HTTPS howto: https://techtutorialsx.com/2017/11/18/esp32-arduino-https-get-request/
const char* rootCACertificate = \
"-----BEGIN CERTIFICATE-----\n" \
"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\n" \
"-----END CERTIFICATE-----\n";

JPEGDEC jpeg;
Pixel_t color_palette[7] = {Pixel_t({0x00, 0x00, 0x00}),
                            Pixel_t({0xFF, 0xFF, 0xFF}),
                            Pixel_t({0x00, 0xFF, 0x00}),
                            Pixel_t({0x00, 0x00, 0xFF}),
                            Pixel_t({0xFF, 0x00, 0x00}),
                            Pixel_t({0xFF, 0xFF, 0x00}),
                            Pixel_t({0xFF, 0x80, 0x00})};
WiFiMulti WiFiMulti;
WiFiClientSecure *client = new WiFiClientSecure;


/* HTTP */
const char *headerkeys[] = {"Location"};

/* Google photo */
char photoIdList[PHOTO_LIMIT][PHOTO_ID_SIZE];
uint8_t *photoBuf;
uint8_t *dispBuf;
int16_t* errors;

/* variables */
static int len, offset, photoCount;
static unsigned long next_show_millis = 0;
static bool shownPhoto = false;

static size_t stream_reader(void *arg, size_t index, uint8_t *buf, size_t len)
{
  Stream *s = (Stream *)arg;
  if (buf)
  {
    // Serial.printf("[HTTP] read: %d\n", len);
    size_t a = s->available();
    size_t r = 0;
    while (r < len)
    {
      r += s->readBytes(buf + r, min((len - r), a));
      delay(50);
    }

    return r;
  }
  else
  {
    // Serial.printf("[HTTP] skip: %d\n", len);
    int l = len;
    while (l--)
    {
      s->read();
    }
    return len;
  }
}

static size_t buffer_reader(void *arg, size_t index, uint8_t *buf, size_t len)
{
  if (buf)
  {
    memcpy(buf, photoBuf + index, len);
  }
  return len;
}





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

void setup() {
  Serial.begin(115200);

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
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(SSID_NAME, SSID_PASSWORD);
  while ((WiFiMulti.run() != WL_CONNECTED)) {
    delay(500);
    Serial.print('.');
  }
  Serial.println(F(" done."));

  client->setCACert(rootCACertificate);

  // set WDT timeout a little bit longer than HTTP timeout
  esp_task_wdt_init((HTTP_TIMEOUT / 1000) + 1, true);
  enableLoopWDT();

  

  // Init epd
  epd_init();
  delay(100);
  Serial.println("epd init done");
}

void loop() {
  if (WiFiMulti.run() != WL_CONNECTED) {
    // wait for WiFi connection
    delay(500);
  }
  else if (millis() < next_show_millis) {
    delay(1000);
  }
  else {
    HTTPClient https;

    if (!photoCount) {
      https.collectHeaders(headerkeys, sizeof(headerkeys) / sizeof(char *));
      Serial.println(F(GOOGLE_PHOTO_SHARE_LINK));
      Serial.println(F("[HTTPS] begin..."));
      https.begin(*client, F(GOOGLE_PHOTO_SHARE_LINK));
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
        delay(9000); // don't repeat the wrong thing too fast
        // return;
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
          // notify WDT still working
          feedLoopWDT();
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
      next_show_millis = ((millis() / 60000L) + 3) * 60000L; // next minute
    }
  }
  // notify WDT still working
  feedLoopWDT();
}


#endif