// Hello world for the Waveshare ESP32-S3-ePaper-1.54 V2:
// draws text on the e-paper, blinks the LED and logs to USB serial.

#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// V2 pins (from Waveshare's ESP-IDF V2 examples)
#define EPD_SCK   12
#define EPD_MOSI  13
#define EPD_CS    11
#define EPD_DC    10
#define EPD_RST    9
#define EPD_BUSY   8
#define EPD_PWR    6   // active low: LOW = display powered
#define VBAT_PWR  17   // HIGH keeps the board on when running from battery
#define LED_PIN    3

// 1.54" 200x200 black/white panel, SSD1681 controller
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

void drawCentered(const char *text, int16_t y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((display.width() - w) / 2 - x1, y);
  display.print(text);
}

void setup() {
  pinMode(VBAT_PWR, OUTPUT);
  digitalWrite(VBAT_PWR, HIGH);
  pinMode(LED_PIN, OUTPUT);
  pinMode(EPD_PWR, OUTPUT);
  digitalWrite(EPD_PWR, LOW);
  delay(10);

  Serial.begin(115200);
  delay(1000);
  Serial.println("hello_epaper: starting");
  Serial.printf("chip: %s rev %d, flash %u MB, PSRAM %u MB\n",
                ESP.getChipModel(), ESP.getChipRevision(),
                ESP.getFlashChipSize() / (1024 * 1024),
                ESP.getPsramSize() / (1024 * 1024));

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  display.init(115200);
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.drawRect(4, 4, 192, 192, GxEPD_BLACK);
    display.setFont(&FreeMonoBold12pt7b);
    drawCentered("Hello,", 80);
    drawCentered("world!", 110);
    display.setFont(&FreeMonoBold9pt7b);
    drawCentered("ESP32-S3 V2", 160);
  } while (display.nextPage());

  display.hibernate();
  Serial.println("hello_epaper: display updated");
}

void loop() {
  static uint32_t n = 0;
  digitalWrite(LED_PIN, n % 2);
  Serial.printf("tick %lu\n", (unsigned long)n++);
  delay(500);
}
