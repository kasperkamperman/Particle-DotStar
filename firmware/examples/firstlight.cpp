#include "application.h"
#include "dotstar/dotstar.h"

#define NUM_LEDS 128

Adafruit_DotStar strip = Adafruit_DotStar(NUM_LEDS, DOTSTAR_BGR);

int counter = 0;

void setup() {

  delay(500);
  Serial.begin(57600);

  strip.begin(); // Initialize pins for output
  //strip.setBrightness(255);
  strip.show();  // Turn all LEDs off ASAP

}

void loop() {

  if(counter>255) {
    counter = 0;
  }

  for(int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, counter, 0, 255-counter);
  }

  counter++;

  strip.show();

  delay(10);
}
