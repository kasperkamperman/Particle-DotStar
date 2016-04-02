/*------------------------------------------------------------------------
  Particle library to control Adafruit DotStar addressable RGB LEDs.

  Ported by Technobly for Particle Core, Photon, P1 and Electron.

  Modified by Kasper

  ------------------------------------------------------------------------
  -- original header follows ---------------------------------------------
  ------------------------------------------------------------------------

  Arduino library to control Adafruit Dot Star addressable RGB LEDs.

  Written by Limor Fried and Phil Burgess for Adafruit Industries.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing products
  from Adafruit!

  ------------------------------------------------------------------------
  This file is part of the Adafruit Dot Star library.

  Adafruit Dot Star is free software: you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public License
  as published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  Adafruit Dot Star is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with NeoPixel.  If not, see <http://www.gnu.org/licenses/>.
  ------------------------------------------------------------------------*/

#include "dotstar.h"

// fast pin access
#if PLATFORM_ID == 0 // Core
  #define pinLO(_pin) (PIN_MAP[_pin].gpio_peripheral->BRR = PIN_MAP[_pin].gpio_pin)
  #define pinHI(_pin) (PIN_MAP[_pin].gpio_peripheral->BSRR = PIN_MAP[_pin].gpio_pin)
#elif PLATFORM_ID == 6 || PLATFORM_ID == 8 || PLATFORM_ID == 10 // Photon (6), P1 (8) or Electron (10)
  STM32_Pin_Info* PIN_MAP2 = HAL_Pin_Map(); // Pointer required for highest access speed
  #define pinLO(_pin) (PIN_MAP2[_pin].gpio_peripheral->BSRRH = PIN_MAP2[_pin].gpio_pin)
  #define pinHI(_pin) (PIN_MAP2[_pin].gpio_peripheral->BSRRL = PIN_MAP2[_pin].gpio_pin)
#else
  #error "*** PLATFORM_ID not supported by this library. PLATFORM should be Core, Photon, P1 or Electron ***"
#endif
// fast pin access
#define pinSet(_pin, _hilo) (_hilo ? pinHI(_pin) : pinLO(_pin))

#define USE_HW_SPI 255 // Assign this to dataPin to indicate 'hard' SPI

bool Adafruit_DotStar::hw_spi_DMA_TransferCompleted;

// Constructor for hardware SPI -- must connect to MOSI, SCK pins
Adafruit_DotStar::Adafruit_DotStar(uint16_t n, uint8_t o, uint8_t s) :
 numLEDs(n), dataPin(USE_HW_SPI), brightness(0), pixels(NULL),
 rOffset(o & 3), gOffset((o >> 2) & 3), bOffset((o >> 4) & 3)
{ if(s==0) use_spi_1 = false;
  else     use_spi_1 = true;
  updateLength(n);
}

// Constructor for 'soft' (bitbang) SPI -- any two pins can be used
Adafruit_DotStar::Adafruit_DotStar(uint16_t n, uint8_t data, uint8_t clock,
  uint8_t o) :
 dataPin(data), clockPin(clock), brightness(0), pixels(NULL),
 rOffset(o & 3), gOffset((o >> 2) & 3), bOffset((o >> 4) & 3)
{
  updateLength(n);
}

Adafruit_DotStar::~Adafruit_DotStar(void) { // Destructor
  if(pixels)                free(pixels);
  if(dataPin == USE_HW_SPI) hw_spi_end();
  else                      sw_spi_end();
}

void Adafruit_DotStar::begin(void) { // Initialize SPI
  if(dataPin == USE_HW_SPI) hw_spi_init();
  else                      sw_spi_init();
}

// Pins may be reassigned post-begin(), so a sketch can store hardware
// config in flash, SD card, etc. rather than hardcoded.  Also permits
// "recycling" LED ram across multiple strips: set pins to first strip,
// render & write all data, reassign pins to next strip, render & write,
// etc.  They won't update simultaneously, but usually unnoticeable.

// Change to hardware SPI -- must connect to MOSI, SCK pins
void Adafruit_DotStar::updatePins(void) {
  sw_spi_end();
  dataPin = USE_HW_SPI;
  hw_spi_init();
}

// Change to 'soft' (bitbang) SPI -- any two pins can be used
void Adafruit_DotStar::updatePins(uint8_t data, uint8_t clock) {
  hw_spi_end();
  dataPin  = data;
  clockPin = clock;
  sw_spi_init();
}

// Length can be changed post-constructor for similar reasons (sketch
// config not hardcoded).  But DON'T use this for "recycling" strip RAM...
// all that reallocation is likely to fragment and eventually fail.
// Instead, set length once to longest strip.
void Adafruit_DotStar::updateLength(uint16_t n) {
  if(pixels) free(pixels);

  // 4 start bytes, 4 bytes for each led
  // For the end frame there are different approaches. Now we use 1 bit (1 clock pulse) for each led.
  // So 1 byte for each 8 leds, because of round down + 1 byte
  uint16_t amountOfEndFrameBytes = (1 + n/8);

  uint16_t bytes = 4 + (n * 4) + amountOfEndFrameBytes;

  if((pixels = (uint8_t *)malloc(bytes))) {

    // set the start bytes
    for(uint16_t i=0; i<4; i++) {
      pixels[i] = 0x00;
    }

    // set the leds to zero
    clear();

    uint16_t endFrameStartPosition = 4 + (n * 4);

    // set the end bytes
    for(uint16_t i = endFrameStartPosition; i<(endFrameStartPosition+amountOfEndFrameBytes); i++) {
      pixels[i] = 0xFF;
    }

    numLEDs = n;
    pixelArrayLength = bytes;


  } else {
    numLEDs = 0;
  }
}

// SPI STUFF ---------------------------------------------------------------

void Adafruit_DotStar::hw_spi_init(void) { // Initialize hardware SPI

  // 72MHz / 4 = 18MHz (sweet spot)
  // Any slower than 18MHz and you are barely faster than Software SPI.
  // Any faster than 18MHz and the code overhead dominates.

  // sweet spot might be different with the Photon since it has a higher clock speed
  // none the less 18Mhz is pretty fast.
  // we fix this instead of depending on the divider.

  if(use_spi_1) {
    // D2/D4
    SPI1.begin();
    SPI1.setClockSpeed(18000000);
    SPI1.setBitOrder(MSBFIRST);
    SPI1.setDataMode(SPI_MODE0);
  } else {
    // A3/A5
    SPI.begin();
    SPI.setClockSpeed(18000000);
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
  };
}

inline void Adafruit_DotStar::hw_spi_end(void) { // Stop hardware SPI
  if(use_spi_1) SPI1.end();
  else          SPI.end();
}

void Adafruit_DotStar::sw_spi_init(void) { // Init 'soft' (bitbang) SPI
  pinMode(dataPin , OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinSet(dataPin , LOW);
  pinSet(clockPin, LOW);
}

void Adafruit_DotStar::sw_spi_end() { // Stop 'soft' SPI
  pinMode(dataPin , INPUT);
  pinMode(clockPin, INPUT);
}

void Adafruit_DotStar::sw_spi_out(uint8_t n) { // Bitbang SPI write
  for(uint8_t i=8; i--; n <<= 1) {
    if(n & 0x80) pinSet(dataPin, HIGH);
    else         pinSet(dataPin, LOW);
    pinSet(clockPin, HIGH);
    pinSet(clockPin, LOW);
  }
}

/* ISSUE DATA TO LED STRIP -------------------------------------------------

  I implemented the per-pixel 5-bit brightness and replaced the scale
  brightness. If you want to scale brightness properly, implement it in your
  sketch.

  Take the comment below in account.

    Although the LED driver has an additional per-pixel 5-bit brightness
    setting, it is NOT used or supported here because it's a brain-dead
    misfeature that's counter to the whole point of Dot Stars, which is to
    have a much faster PWM rate than NeoPixels.  It gates the high-speed
    PWM output through a second, much slower PWM (about 400 Hz), rendering
    it useless for POV.  This brings NOTHING to the table that can't be
    already handled better in one's sketch code.
*/

void Adafruit_DotStar::hw_spi_DMA_TransferComplete_Callback(void) {
    hw_spi_DMA_TransferCompleted = true;
}

void Adafruit_DotStar::show(void) {

  if(!pixels) return;

  //__disable_irq(); // If 100% focus on SPI clocking required

  if(dataPin == USE_HW_SPI) {

    // Big change here
    // Photon supports DMA if we send the whole pixel array.
    // See: https://docs.particle.io/reference/firmware/photon/#transfer-
    // That's why the pixel array layout is changed.

    // brightness scaling is ignored for now (was applied here before and still in soft_spi)

    // transferComplete_Callback method from this topic:
    // https://community.particle.io/t/bug-in-spi-block-transfer-complete-callback/18568

    hw_spi_DMA_TransferCompleted = false;

    if(!use_spi_1) SPI.transfer((void *)pixels, 0, pixelArrayLength, hw_spi_DMA_TransferComplete_Callback);
    else           SPI1.transfer((void *)pixels, 0, pixelArrayLength, hw_spi_DMA_TransferComplete_Callback);

    //SPI.transfer((void *)pixels, 0, pixelArrayLength, NULL);

    // we wait on the callback now to measure the APA102 update time
    // decomment if you want to continue (DMA happens in background)
    while(!hw_spi_DMA_TransferCompleted);

  } else {                               // Soft (bitbang) SPI

    // because of the new pixel layout we can output the array
    // in one time. Brightness scaling is removed.
    for(int i=0; i<pixelArrayLength; i++) {
      sw_spi_out(pixels[i]);
    };
  }

  //__enable_irq();
}

inline void Adafruit_DotStar::clear() {
  // was memset before now we just make the pixels black.

  for(uint16_t i = 0;i<numLEDs;i++) {
    setPixelColor(i,0,0,0);
  }
}

// Set pixel color, separate R,G,B values (0-255 ea.)
void Adafruit_DotStar::setPixelColor(
 uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
  if(n < numLEDs) {
    // we include
    uint8_t *p = &pixels[4 + (n * 4)];

    //we apply 5-bit brightness later...
    //0xE0 + (brightness>>3);
    p[0]         = 0xE0 + (brightness>>3);
    p[rOffset+1] = r;
    p[gOffset+1] = g;
    p[bOffset+1] = b;
  }
}

// Set pixel color, 'packed' RGB value (0x000000 - 0xFFFFFF)
void Adafruit_DotStar::setPixelColor(uint16_t n, uint32_t c) {
  if(n < numLEDs) {
    uint8_t *p = &pixels[n * 4];
    p[0]         = 0xFF;
    p[rOffset+1] = (uint8_t)(c >> 16);
    p[gOffset+1] = (uint8_t)(c >>  8);
    p[bOffset+1] = (uint8_t)c;
  }
}

// Convert separate R,G,B to packed value
uint32_t Adafruit_DotStar::Color(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Read color from previously-set pixel, returns packed RGB value.
uint32_t Adafruit_DotStar::getPixelColor(uint16_t n) const {
  if(n >= numLEDs) return 0;

  // 4 start bytes, the 1st parameter is the pixel brightness
  // we don't give that back.
  uint8_t *p = &pixels[4 + (n * 4) + 1];

  return ((uint32_t)p[rOffset] << 16) |
         ((uint32_t)p[gOffset] <<  8) |
          (uint32_t)p[bOffset];
}

uint16_t Adafruit_DotStar::numPixels(void) { // Ret. strip length
  return numLEDs;
}

// Set global strip brightness. This does not have an immediate effect;
// we use the pixel brightness parameter of the APA102/DotStar strips for this.
// use the orignal library if you don't want that.

// must be followed by a call to show().  Not a fan of this...for various
// reasons I think it's better handled in one's sketch, but it's here for
// parity with the NeoPixel library.  Good news is that brightness setting
// in this library is 'non destructive' -- it's applied as color data is
// being issued to the strip, not during setPixel(), and also means that
// getPixelColor() returns the exact value originally stored.

inline void Adafruit_DotStar::setBrightness(uint8_t b) {
  // now we use apa102 pixel brightness, so above doesn't apply
  brightness = b;
}

inline uint8_t Adafruit_DotStar::getBrightness(void) const {
  return brightness;
}

// Return pointer to the library's pixel data buffer.  Use carefully,
// much opportunity for mayhem.  It's mostly for code that needs fast
// transfers, e.g. SD card to LEDs.  Color data is in BGR order.
inline uint8_t *Adafruit_DotStar::getPixels(void) const {
  return pixels;
}
