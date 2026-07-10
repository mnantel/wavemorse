#pragma once
#include <Arduino_GFX_Library.h>

#include "config.h"

// 1.47" ST7789, 172x320, used in landscape (320x172).
// Layout: status bar / in-progress pattern / scrolling decoded text.
class Display {
public:
  bool begin() {
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
    _bus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_SCK,
                                PIN_LCD_MOSI, GFX_NOT_DEFINED);
    _gfx = new Arduino_ST7789(_bus, PIN_LCD_RST, 1 /* landscape */,
                              true /* IPS */, 172, 320, 34, 0, 34, 0);
    bool ok = _gfx->begin();
    // brief RGB self-test so a working panel is obvious at power-on
    _gfx->fillScreen(RGB565_RED);
    delay(250);
    _gfx->fillScreen(RGB565_GREEN);
    delay(250);
    _gfx->fillScreen(RGB565_BLUE);
    delay(250);
    _gfx->fillScreen(RGB565_BLACK);
    return ok;
  }

  void status(uint8_t wpm, bool iambicB, bool midiPaddle, bool usb,
              bool straightKey = false) {
    _gfx->fillRect(0, 0, 320, 20, RGB565_DARKCYAN);
    _gfx->setTextColor(RGB565_WHITE);
    _gfx->setTextSize(2);
    _gfx->setCursor(4, 3);
    char buf[32];
    snprintf(buf, sizeof(buf), "%2dwpm %c %s %s", wpm, iambicB ? 'B' : 'A',
             straightKey ? "SKEY" : (midiPaddle ? "PADDLE" : "KEYER"),
             usb ? "USB" : "---");
    _gfx->print(buf);
  }

  void pattern(const char *pat) {
    _gfx->fillRect(0, 24, 320, 34, RGB565_BLACK);
    _gfx->setTextColor(RGB565_YELLOW);
    _gfx->setTextSize(4);
    _gfx->setCursor(4, 26);
    _gfx->print(pat);
  }

  void append(char c) {
    size_t n = strlen(_text);
    if (n >= sizeof(_text) - 1) {
      // drop the oldest line worth of text
      memmove(_text, _text + COLS, n - COLS + 1);
      n -= COLS;
    }
    _text[n] = c;
    _text[n + 1] = 0;
    render();
  }

  void clearText() {
    _text[0] = 0;
    render();
  }

private:
  static const int COLS = 26; // 320 / 12px per size-2 char
  static const int ROWS = 6;
  static const int TEXT_Y = 64;

  void render() {
    _gfx->fillRect(0, TEXT_Y, 320, 172 - TEXT_Y, RGB565_BLACK);
    _gfx->setTextColor(RGB565_GREEN);
    _gfx->setTextSize(2);
    size_t len = strlen(_text);
    size_t lines = (len + COLS - 1) / COLS;
    size_t firstLine = lines > ROWS ? lines - ROWS : 0;
    int y = TEXT_Y + 2;
    for (size_t l = firstLine; l < lines; l++) {
      _gfx->setCursor(2, y);
      for (size_t i = l * COLS; i < min(len, (l + 1) * COLS); i++)
        _gfx->write(_text[i]);
      y += 18;
    }
  }

  Arduino_DataBus *_bus = nullptr;
  Arduino_GFX *_gfx = nullptr;
  char _text[COLS * ROWS + 1] = {0};
};
