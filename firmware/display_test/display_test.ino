// Pod: display + touch test
// Checks the 2.8" ILI9341 screen and XPT2046 touch on the shared SPI0 bus.
//
// What you should see:
//   1. Backlight turns on, screen flashes red, green, blue
//   2. A "Pod" title screen with the words "Touch me"
//   3. Wherever you press, a dot appears under your finger
//   4. The bottom line shows touch pressure vs the threshold (green = counted as a touch)
//
// Wiring (Pico 2 W):
//   Display: CS GP17, DC GP16, RESET GP21, MOSI GP19, SCK GP18, LED GP13, MISO not connected
//   Touch:   T_CS GP22, T_DIN GP19, T_CLK GP18, T_DO GP20, T_IRQ GP26

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <PodTouch.h>

const int PIN_SCK = 18, PIN_MOSI = 19, PIN_MISO = 20;
const int TFT_CS = 17, TFT_DC = 16, TFT_RST = 21, TFT_LED = 13;
const int TOUCH_CS = 22, TOUCH_IRQ = 26;

Adafruit_ILI9341 tft(&SPI, TFT_DC, TFT_CS, TFT_RST);
PodTouch touch(TOUCH_CS);

void drawHome() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(5);
  tft.setCursor(60, 100);
  tft.print("Pod");
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_LIGHTGREY);
  tft.setCursor(66, 170);
  tft.print("Touch me");
}

void setup() {
  Serial.begin(115200);

  // Both chips share SPI0; tell the Pico which pins it uses
  SPI.setSCK(PIN_SCK);
  SPI.setTX(PIN_MOSI);
  SPI.setRX(PIN_MISO);

  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);   // backlight on

  tft.begin();
  tft.setRotation(0);            // portrait, 240 x 320, header pins at the top

  tft.fillScreen(ILI9341_RED);   delay(400);
  tft.fillScreen(ILI9341_GREEN); delay(400);
  tft.fillScreen(ILI9341_BLUE);  delay(400);

  touch.begin();
  touch.threshold = 60;    // lower = lighter touch; the old library used 300

  drawHome();
  Serial.println("Display test ready - touch the screen");
}

void loop() {
  PodTouchPoint p;
  bool pressed = touch.read(p);

  // Live pressure readout so we can tune the threshold
  static unsigned long lastShown = 0;
  if (millis() - lastShown > 100) {
    lastShown = millis();
    tft.fillRect(0, 296, 240, 24, ILI9341_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(pressed ? ILI9341_GREEN : ILI9341_DARKGREY);
    tft.setCursor(4, 300);
    tft.printf("press %4d / %d", p.pressure, touch.threshold);
  }

  if (!pressed) return;
  tft.fillCircle(p.x, p.y, 4, ILI9341_YELLOW);
  Serial.printf("pressure=%d raw=%d,%d screen=%d,%d\n", p.pressure, p.rawX, p.rawY, p.x, p.y);
  delay(10);
}
