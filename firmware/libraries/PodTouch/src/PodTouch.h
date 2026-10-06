// PodTouch: XPT2046 resistive touch driver for Pod
//
// Why not XPT2046_Touchscreen? That library ignores any press with a pressure
// reading below a fixed 300, so light taps don't register. PodTouch makes the
// threshold adjustable, takes more samples to keep light touches steady, and
// converts raw readings straight into screen pixels using Pod's calibration.

#pragma once
#include <Arduino.h>
#include <SPI.h>

struct PodTouchPoint {
  int x, y;          // screen pixels (portrait, 240 x 320)
  int rawX, rawY;    // raw 0..4095 readings, same orientation as XPT2046_Touchscreen rotation 0
  int pressure;      // higher = harder press; 0 = not touched
};

class PodTouch {
public:
  // Pressure needed to count as a touch. Lower = lighter touch, but too low
  // picks up noise. The old library used 300.
  int threshold = 120;

  // How many back-to-back readings must agree before a touch counts (stops ghost taps)
  int confirmReads = 2;

  // Calibration, measured on Pod's board (2026-10-07). Both axes run backwards.
  int rawXLeft = 3540, rawXRight = 565;
  int rawYTop  = 3680, rawYBottom = 380;
  int width = 240, height = 320;

  PodTouch(uint8_t csPin, SPIClass &spi = SPI) : _cs(csPin), _spi(&spi) {}

  void begin() {
    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
  }

  // Returns true while the screen is pressed, and fills in p.
  bool read(PodTouchPoint &p) {
    int pressure, rx, ry;
    sample(pressure, rx, ry);
    p.pressure = pressure;

    if (pressure < threshold) {
      _streak = 0;
      return false;
    }
    if (++_streak < confirmReads) return false;

    // Average a few more samples for a steadier position on light presses
    long sx = rx, sy = ry; int n = 1;
    for (int i = 0; i < 3; i++) {
      int pr, x2, y2;
      sample(pr, x2, y2);
      if (pr >= threshold) { sx += x2; sy += y2; n++; }
    }
    p.rawX = sx / n;
    p.rawY = sy / n;
    p.x = constrain(map(p.rawX, rawXLeft, rawXRight, 0, width - 1), 0, width - 1);
    p.y = constrain(map(p.rawY, rawYTop, rawYBottom, 0, height - 1), 0, height - 1);
    return true;
  }

private:
  uint8_t _cs;
  SPIClass *_spi;
  int _streak = 0;

  static int16_t bestTwoAvg(int16_t a, int16_t b, int16_t c) {
    int16_t dab = abs(a - b), dac = abs(a - c), dbc = abs(b - c);
    if (dab <= dac && dab <= dbc) return (a + b) >> 1;
    if (dac <= dab && dac <= dbc) return (a + c) >> 1;
    return (b + c) >> 1;
  }

  // One measurement: pressure plus position (same maths as XPT2046_Touchscreen)
  void sample(int &pressure, int &rawX, int &rawY) {
    int16_t d[6];
    _spi->beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_cs, LOW);
    _spi->transfer(0xB1);                         // Z1
    int16_t z1 = _spi->transfer16(0xC1) >> 3;     // Z2
    int z = z1 + 4095;
    int16_t z2 = _spi->transfer16(0x91) >> 3;
    z -= z2;
    _spi->transfer16(0x91);                       // first reading is noisy; discard
    d[0] = _spi->transfer16(0xD1) >> 3;
    d[1] = _spi->transfer16(0x91) >> 3;
    d[2] = _spi->transfer16(0xD1) >> 3;
    d[3] = _spi->transfer16(0x91) >> 3;
    d[4] = _spi->transfer16(0xD0) >> 3;           // last reading, then power down
    d[5] = _spi->transfer16(0) >> 3;
    digitalWrite(_cs, HIGH);
    _spi->endTransaction();

    pressure = z < 0 ? 0 : z;
    int16_t x = bestTwoAvg(d[0], d[2], d[4]);
    int16_t y = bestTwoAvg(d[1], d[3], d[5]);
    rawX = 4095 - y;   // rotation 0, matches the old library so calibration still holds
    rawY = x;
  }
};
