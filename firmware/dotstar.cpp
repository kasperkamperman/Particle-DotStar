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

#define spi_out(n) (void)SPI.transfer(n)

#define USE_HW_SPI 255 // Assign this to dataPin to indicate 'hard' SPI

// Constructor for hardware SPI -- must connect to MOSI, SCK pins
Adafruit_DotStar::Adafruit_DotStar(uint16_t n, uint8_t o) :
 numLEDs(n), dataPin(USE_HW_SPI), brightness(0), pixels(NULL),
 rOffset(o & 3), gOffset((o >> 2) & 3), bOffset((o >> 4) & 3)
{
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
  SPI.begin();
  // 72MHz / 4 = 18MHz (sweet spot)
  // Any slower than 18MHz and you are barely faster than Software SPI.
  // Any faster than 18MHz and the code overhead dominates.

  // sweet spot might be different with the Photon since it has a higher clock speed
  // none the less 18Mhz is pretty fast.
  // we fix this instead of depending on the divider.

  //SPI.setClockDivider(SPI_CLOCK_DIV4);
  SPI.setClockSpeed(18000000);
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
}

inline void Adafruit_DotStar::hw_spi_end(void) { // Stop hardware SPI
  SPI.end();
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

  Although the LED driver has an additional per-pixel 5-bit brightness
  setting, it is NOT used or supported here because it's a brain-dead
  misfeature that's counter to the whole point of Dot Stars, which is to
  have a much faster PWM rate than NeoPixels.  It gates the high-speed
  PWM output through a second, much slower PWM (about 400 Hz), rendering
  it useless for POV.  This brings NOTHING to the table that can't be
  already handled better in one's sketch code.  If you really can't live
  without this abomination, you can fork the library and add it for your
  own use, but any pull requests for this will NOT be merged, nuh uh!
*/

void Adafruit_DotStar::hw_spi_DMA_TransferComplete_Callback(void) {
    hw_spi_DMA_TransferCompleted = true;
}

void Adafruit_DotStar::show(void) {

  if(!pixels) return;

  uint8_t *ptr = pixels, i;            // -> LED data
  uint16_t n   = numLEDs;              // Counter
  uint16_t b16 = (uint16_t)brightness; // Type-convert for fixed-point math

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
    // line below gives error: invalid use of non-static member function
    //SPI.transfer((void *)pixels, 0, pixelArrayLength, hw_spi_DMA_TransferComplete_Callback);
    SPI.transfer((void *)pixels, 0, pixelArrayLength, NULL);

    // we wait on the callback now to measure the APA102 update time
    // decomment if you want to continue (DMA happens in background)
    //while(!hw_spi_DMA_TransferCompleted);

  } else {                               // Soft (bitbang) SPI

    // to implement
    // this doesn't fit the new pixel array layout

    /*
    for(i=0; i<4; i++) sw_spi_out(0);    // Start-frame marker

    if(brightness) {                     // Scale pixel brightness on output
      do {                               // For each pixel...
        sw_spi_out(0xFF);                //  Pixel start
        for(i=0; i<3; i++) sw_spi_out((*ptr++ * b16) >> 8); // Scale, write
      } while(--n);
    } else {                             // Full brightness (no scaling)
      do {                               // For each pixel...
        sw_spi_out(0xFF);                //  Pixel start
        for(i=0; i<3; i++) sw_spi_out(*ptr++); // R,G,B
      } while(--n);
    }
    for(i=0; i<4; i++) sw_spi_out(0xFF); // End-frame marker (see note above)
    */
  }

  //__enable_irq();
}




inline void Adafruit_DotStar::clear() { // Write 0s (off) to full pixel buffer
  //memset(pixels, 4, numLEDs * 4);
  //we only need to write 0s to the pixels not to start/end frame...

  // fix bug, still steps of 4...
  /*for(uint16_t i = 4;i<numLEDs;i=i+4) {
    pixels[i]   = 0xFF;
    pixels[i+1] = 0;
    pixels[i+2] = 0;
    pixels[i+3] = 0;
  }*/
}

// Set pixel color, separate R,G,B values (0-255 ea.)
void Adafruit_DotStar::setPixelColor(
 uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
  if(n < numLEDs) {
    // we include
    uint8_t *p = &pixels[4 + (n * 4)];

    //we apply 5-bit brightness later...
    //0xE0 + (brightness>>3);
    p[0]         = 0xFF;
    p[rOffset+1] = r;
    p[gOffset+1] = g;
    p[bOffset+1] = b;
  }
}

// Set pixel color, 'packed' RGB value (0x000000 - 0xFFFFFF)
void Adafruit_DotStar::setPixelColor(uint16_t n, uint32_t c) {
  if(n < numLEDs) {
    uint8_t *p = &pixels[n * 3];
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
  //if(n >= numLEDs)
    return 0;

  // not implemented in new pixel layout
  /*
  uint8_t *p = &pixels[n * 3];

  return ((uint32_t)p[rOffset] << 16) |
         ((uint32_t)p[gOffset] <<  8) |
          (uint32_t)p[bOffset];
  */
}

uint16_t Adafruit_DotStar::numPixels(void) { // Ret. strip length
  return numLEDs;
}

// Set global strip brightness.  This does not have an immediate effect;
// must be followed by a call to show().  Not a fan of this...for various
// reasons I think it's better handled in one's sketch, but it's here for
// parity with the NeoPixel library.  Good news is that brightness setting
// in this library is 'non destructive' -- it's applied as color data is
// being issued to the strip, not during setPixel(), and also means that
// getPixelColor() returns the exact value originally stored.
inline void Adafruit_DotStar::setBrightness(uint8_t b) {
  // Stored brightness value is different than what's passed.  This
  // optimizes the actual scaling math later, allowing a fast 8x8-bit
  // multiply and taking the MSB.  'brightness' is a uint8_t, adding 1
  // here may (intentionally) roll over...so 0 = max brightness (color
  // values are interpreted literally; no scaling), 1 = min brightness
  // (off), 255 = just below max brightness.
  //brightness = b + 1;

  // now we use apa102 pixel brightness, so above doesn't apply
  brightness = b;
}

inline uint8_t Adafruit_DotStar::getBrightness(void) const {
  return brightness; // - 1; // Reverse above operation
}

// Return pointer to the library's pixel data buffer.  Use carefully,
// much opportunity for mayhem.  It's mostly for code that needs fast
// transfers, e.g. SD card to LEDs.  Color data is in BGR order.
inline uint8_t *Adafruit_DotStar::getPixels(void) const {
  return pixels;
}
