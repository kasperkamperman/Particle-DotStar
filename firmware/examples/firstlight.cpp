#include "application.h"
#include "dotstar/dotstar.h"

#define NUM_LEDS 128

Adafruit_DotStar strip = Adafruit_DotStar(NUM_LEDS, DOTSTAR_BGR);

// debug duration
unsigned long t1; // for measuring purposes
unsigned long t2; // for measuring purposes
unsigned long t3 = 0; // for measuring purposes

int counter = 0;

void setup() {

  delay(500);
  Serial.begin(57600);

  strip.begin(); // Initialize pins for output
  strip.show();  // Turn all LEDs off ASAP

}

void setTime() {
   t1 = micros();
}

void printTime() {
    t2 = micros()-t1;
    if(t2>t3) t3 = t2;

    Serial.print(F("update time: "));
    Serial.print(t3);
    Serial.print(" ");
    Serial.println(t2);
}

void loop() {

  if(counter>255) {
    counter = 0;
  }

  for(int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, counter, 0, 255-counter);
  }

  counter++;

  setTime();
    strip.show();
  printTime();

  delay(200);
}
