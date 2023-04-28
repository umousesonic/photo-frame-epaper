#ifdef TEST_DISPLAY
#include <Arduino.h>
#include "epd/epd.hpp"
#include "ImageData.h"

void setup() {
  Serial.begin(115200);
  Serial.println("Begin setup");
  epd_init();
  delay(100);
  Serial.println("Begin display");
  epd_display_fullimage(gImage_5in65f_test);
  Serial.println("Image should be there.");
  delay(5000);
  Serial.println("Begin clear");
  epd_clear(EPD_WHITE);
  Serial.println("Waiting for clear to finish");
  delay(100);
  Serial.println("Begin sleep");
  epd_sleep();
}

void loop() {

}

#endif