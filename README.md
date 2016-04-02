Particle-DotStar Mod
====================

This fork was made for people that have a need for speed. 

The original library takes about 800us for 128 leds (hardware SPI), so this saves
about 1ms. 800us is still very fast, so beginners better might use the original library. 

This fork has several changes. 

SPI.transfer was modified in order to support build in DMA. This transfers 
the pixel data to the leds in the background, so you continue with upcoming 
calculations, see [Particle Reference] (https://docs.particle.io/reference/firmware/photon/#transfer-).

To do that the internal layout of the pixels array was changed. 

setBrightness() now modifies the 5-bit brightness parameter of the DotStar leds. This 
is not the most optimimal, so again a reason for beginners to skip this. It's a number
between 0-255 that will be scaled down to 0-31 (5-bit). 

The software SPI part wasn't really changed (although it bitbangs the new layout)

If you'd like to use SPI use the following pins on the Photon:
- A3 Clock
- A5 Data

alternatively you can use SPI1:
- D4 Clock
- D2 Data

Hardware SPI calls:

```cpp
Adafruit_DotStar strip = Adafruit_DotStar(NUM_LEDS); // SPI (A3/A5)
Adafruit_DotStar strip = Adafruit_DotStar(NUM_LEDS, DOTSTAR_BGR, 0); // SPI (A3/A5)
Adafruit_DotStar strip = Adafruit_DotStar(NUM_LEDS, DOTSTAR_BGR, 1); // SPI1 (D2/D4)

```

Original library text below
===========================


A library for manipulating DotStar RGB LEDs for the Spark Core, Particle Photon, P1 and Electron.
Implementation based on Adafruit's DotStar Library.

DotStar LED's are APA102: [Datasheet](http://www.adafruit.com/datasheets/APA102.pdf)

Components Required
---
- A DotStar digital RGB LED (get at [adafruit.com](http://www.adafruit.com/search?q=dotstar&b=1))
- A Particle Shield Shield or breakout board to supply DotStars with 5V (see store at [particle.io](particle.io))

Example Usage
---

```cpp
Adafruit_DotStar strip = Adafruit_DotStar(NUMPIXELS, DATAPIN, CLOCKPIN);
void setup() {
  strip.begin();
  strip.show();
}
void loop() {
  // change your pixel colors and call strip.show() again
}
```

Nuances
---

- Make sure get the # of pixels, clock and data pin numbers (SW SPI can be any pins, HW SPI can only be A3 (clock) & A5 (data))

- DotStars require 5V logic level inputs and the Spark Core/Photon/P1/Electron only have 3.3V logic level digital outputs. Level shifting from 3.3V to 5V is
necessary, the Spark Shield Shield has the [TXB0108PWR](http://www.digikey.com/product-search/en?pv7=2&k=TXB0108PWR) 3.3V to 5V level shifter built in (but has been known to oscillate at 50MHz with wire length longer than 6"), alternatively you can wire up your own with a [SN74HCT245N](http://www.digikey.com/product-detail/en/SN74HCT245N/296-1612-5-ND/277258), or [SN74HCT125N](http://www.digikey.com/product-detail/en/SN74HCT125N/296-8386-5-ND/376860). These are rock solid.


Useful Links
---

- DotStar Guide: https://learn.adafruit.com/adafruit-dotstar-leds
- Quad Level Shifter IC: [SN74ACHT125N](https://www.adafruit.com/product/1787) (Adafruit)
- Quad Level Shifter IC: [SN74HCT125N](http://www.digikey.com/product-detail/en/SN74HCT125N/296-8386-5-ND/376860) (Digikey)
- Quad Level Shifter IC: [SN74AHCT125N](http://www.digikey.com/product-detail/en/SN74AHCT125N/296-4655-5-ND/375798) (Digikey)
