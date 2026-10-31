// Philolog on the Waveshare ESP32-S3-ePaper-1.54 V2.
//
// Shows one philosophy nugget a day from philolog's notes/nuggets.fr.md.
// At 08:00 (Paris) the board wakes, draws the day's quote and goes back to
// deep sleep; the quote stays on the e-paper with no power. Press BOOT or PWR
// to page through the rest of the nugget: title and explanation, "À essayer",
// source. After the last page, or 2 minutes without a press, it goes back to
// the quote. Hold PWR for 3 s to switch off when running on battery.
//
// Time comes from the PCF85063 RTC. If the RTC has lost its time, it is set
// over Wi-Fi (NTP) when secrets.h exists, otherwise from the build time. You
// can also set it over USB serial by sending "T<unix epoch>" while awake.

#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <vector>
#include <sys/time.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

#include "line.h"
#include "nuggets.h"
#if __has_include("secrets.h")
#include "secrets.h"  // #define WIFI_SSID "..." and WIFI_PASS "..."
#endif

// V2 pins (from Waveshare's ESP-IDF V2 examples)
#define EPD_SCK    12
#define EPD_MOSI   13
#define EPD_CS     11
#define EPD_DC     10
#define EPD_RST     9
#define EPD_BUSY    8
#define EPD_PWR     6   // active low
#define VBAT_PWR   17   // HIGH keeps the board on when running from battery
#define AUDIO_PWR  42   // active low; also powers the I2C bus (RTC, sensor)
#define BTN_BOOT    0   // active low
#define BTN_PWR    18   // active low
#define I2C_SDA    47
#define I2C_SCL    48
#define PCF85063_ADDR 0x51

// Jour 1 is shown from START_DATE until the next morning; the cycle repeats.
const int START_YEAR = 2026, START_MONTH = 10, START_DAY = 31;
const int WAKE_HOUR = 8;                         // new nugget every day at 08:00
const char *TZ_PARIS = "CET-1CEST,M3.5.0,M10.5.0/3";
const uint32_t AWAKE_MS = 20 * 1000;             // stay awake after a press
const uint32_t BACK_TO_QUOTE_S = 2 * 60;         // idle time before showing the quote again
const uint32_t POWER_OFF_HOLD_MS = 3000;

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
U8G2_FOR_ADAFRUIT_GFX u8g2;

// Survives deep sleep
RTC_DATA_ATTR int shownDay = -1;  // nugget index on screen, -1 = none yet
RTC_DATA_ATTR int page = 0;       // 0 = quote, 1.. = detail pages

bool drewThisSession = false;     // first draw after wake is a full refresh
String rtcDiag;                   // what the RTC returned at boot, printed by the "R" command

// ---------- RTC (PCF85063) ----------

uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

time_t buildTime() {
  // __DATE__ is "Oct 31 2026", __TIME__ is "17:51:07", in the build machine's local time
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {};
  struct tm t = {};
  sscanf(__DATE__, "%3s %d %d", mon, &t.tm_mday, &t.tm_year);
  sscanf(__TIME__, "%d:%d:%d", &t.tm_hour, &t.tm_min, &t.tm_sec);
  t.tm_mon = (strstr(months, mon) - months) / 3;
  t.tm_year -= 1900;
  t.tm_isdst = -1;
  return mktime(&t);
}

// Returns false if the RTC is missing or its oscillator stopped (time lost).
bool rtcRead(time_t &out) {
  Wire.beginTransmission(PCF85063_ADDR);
  Wire.write(0x00);
  int err = Wire.endTransmission(false);
  if (err != 0) {
    rtcDiag = "i2c error " + String(err);
    return false;
  }
  if (Wire.requestFrom(PCF85063_ADDR, 11) != 11) {
    rtcDiag = "short read";
    return false;
  }
  uint8_t all[11];
  for (int i = 0; i < 11; i++) all[i] = Wire.read();
  rtcDiag = "regs";
  for (int i = 0; i < 11; i++) rtcDiag += " " + String(all[i], HEX);
  uint8_t *r = all + 4;
  if (all[0] & 0x20) return false;  // STOP bit: oscillator halted
  if (r[0] & 0x80) return false;    // OS flag: clock integrity not guaranteed
  struct tm t = {};
  t.tm_sec = bcd2dec(r[0] & 0x7F);
  t.tm_min = bcd2dec(r[1] & 0x7F);
  t.tm_hour = bcd2dec(r[2] & 0x3F);
  t.tm_mday = bcd2dec(r[3] & 0x3F);
  t.tm_mon = bcd2dec(r[5] & 0x1F) - 1;
  t.tm_year = bcd2dec(r[6]) + 100;
  // The RTC holds UTC
  setenv("TZ", "UTC0", 1);
  tzset();
  out = mktime(&t);
  setenv("TZ", TZ_PARIS, 1);
  tzset();
  // Reject leftovers from other firmware, e.g. a 2056 date with a clear OS flag
  time_t built = buildTime();
  return out > built - 86400 && out < built + 20L * 365 * 86400;
}

void rtcWrite(time_t now) {
  struct tm t;
  gmtime_r(&now, &t);
  // Clear STOP and select 24-hour mode, keeping the other Control_1 bits
  Wire.beginTransmission(PCF85063_ADDR);
  Wire.write(0x00);
  Wire.endTransmission(false);
  uint8_t ctrl = Wire.requestFrom(PCF85063_ADDR, 1) == 1 ? Wire.read() : 0;
  Wire.beginTransmission(PCF85063_ADDR);
  Wire.write(0x00);
  Wire.write(ctrl & ~0x22);
  Wire.endTransmission();

  Wire.beginTransmission(PCF85063_ADDR);
  Wire.write(0x04);
  Wire.write(dec2bcd(t.tm_sec));  // also clears the OS flag
  Wire.write(dec2bcd(t.tm_min));
  Wire.write(dec2bcd(t.tm_hour));
  Wire.write(dec2bcd(t.tm_mday));
  Wire.write(t.tm_wday);
  Wire.write(dec2bcd(t.tm_mon + 1));
  Wire.write(dec2bcd(t.tm_year - 100));
  Wire.endTransmission();
}

void setSystemTime(time_t now) {
  struct timeval tv = {now, 0};
  settimeofday(&tv, nullptr);
}


bool ntpTime(time_t &out) {
#ifdef WIFI_SSID
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(250);
  if (WiFi.status() == WL_CONNECTED) {
    configTzTime(TZ_PARIS, "pool.ntp.org", "time.google.com");
    struct tm t;
    if (getLocalTime(&t, 10000)) {
      out = time(nullptr);
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      return true;
    }
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
#endif
  return false;
}

void syncClock() {
  time_t now;
  if (rtcRead(now)) {
    setSystemTime(now);
    return;
  }
  if (ntpTime(now)) {
    Serial.println("clock: set from NTP");
  } else if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED) {
    // The ESP32's own clock keeps running through deep sleep: trust it over the build time
    now = time(nullptr);
  } else {
    now = buildTime();
    Serial.println("clock: RTC invalid, using build time");
  }
  setSystemTime(now);
  rtcWrite(now);
}

// ---------- Day logic ----------

// Days since START_DATE, where each day starts at WAKE_HOUR.
int nuggetIndex(time_t now) {
  time_t shifted = now - WAKE_HOUR * 3600;
  struct tm t;
  localtime_r(&shifted, &t);
  // Count whole calendar days using noon to stay clear of DST changes
  struct tm a = {}, b = {};
  a.tm_year = START_YEAR - 1900; a.tm_mon = START_MONTH - 1; a.tm_mday = START_DAY; a.tm_hour = 12;
  b.tm_year = t.tm_year; b.tm_mon = t.tm_mon; b.tm_mday = t.tm_mday; b.tm_hour = 12;
  a.tm_isdst = b.tm_isdst = -1;
  long days = lround(difftime(mktime(&b), mktime(&a)) / 86400.0);
  if (days < 0) days = 0;
  return days % NUGGET_COUNT;
}

time_t nextWake(time_t now) {
  struct tm t;
  localtime_r(&now, &t);
  t.tm_hour = WAKE_HOUR; t.tm_min = 0; t.tm_sec = 5; t.tm_isdst = -1;
  time_t wake = mktime(&t);
  if (wake <= now) {
    t.tm_mday += 1;
    t.tm_isdst = -1;
    wake = mktime(&t);
  }
  return wake;
}

// ---------- Layout ----------

const int MARGIN = 8;
const int WIDTH = 200 - 2 * MARGIN;

int lineHeight(const uint8_t *font) {
  u8g2.setFont(font);
  return u8g2.getFontAscent() - u8g2.getFontDescent() + 3;
}

String forDisplay(String s) {
  s.replace("\xC2\xA0", " ");  // non-breaking spaces only matter for wrapping
  return s;
}

// Word-wraps text into lines that fit WIDTH, breaking only on plain spaces.
void wrap(const char *text, const uint8_t *font, int gapBefore, std::vector<Line> &out) {
  u8g2.setFont(font);
  String line, word;
  String rest = String(text) + " ";
  bool first = true;
  for (unsigned i = 0; i < rest.length(); i++) {
    char c = rest[i];
    if (c != ' ') {
      word += c;
      continue;
    }
    String candidate = line.length() ? line + " " + word : word;
    if (line.length() && u8g2.getUTF8Width(forDisplay(candidate).c_str()) > WIDTH) {
      out.push_back({font, line, first ? gapBefore : 0});
      first = false;
      line = word;
    } else {
      line = candidate;
    }
    word = "";
  }
  if (line.length()) out.push_back({font, line, first ? gapBefore : 0});
}

// Splits detail lines into pages that fit above the page number.
std::vector<std::vector<Line>> paginate(const std::vector<Line> &lines) {
  const int usable = 200 - 2 * MARGIN - 12;
  std::vector<std::vector<Line>> pages(1);
  int y = 0;
  for (const Line &l : lines) {
    int gap = pages.back().empty() ? 0 : l.gapBefore;
    int h = gap + lineHeight(l.font);
    if (y + h > usable && !pages.back().empty()) {
      pages.emplace_back();
      y = 0;
      h = lineHeight(l.font);
    }
    pages.back().push_back(l);
    y += h;
  }
  return pages;
}

std::vector<std::vector<Line>> detailPages(const Nugget &n) {
  std::vector<Line> lines;
  wrap(n.title, u8g2_font_ncenB10_tf, 0, lines);
  wrap(n.body, u8g2_font_ncenR10_tf, 8, lines);
  String essayer = String("À essayer :\xC2\xA0") + n.essayer;
  wrap(essayer.c_str(), u8g2_font_ncenB10_tf, 8, lines);
  wrap(n.source, u8g2_font_helvR08_tf, 8, lines);
  return paginate(lines);
}

// ---------- Drawing ----------

void beginDraw() {
  if (drewThisSession) {
    display.setPartialWindow(0, 0, display.width(), display.height());
  } else {
    display.setFullWindow();
  }
  display.firstPage();
}

void drawQuote(const Nugget &n) {
  // Pick the largest font the quote fits in
  static const uint8_t *fonts[] = {u8g2_font_ncenR18_tf, u8g2_font_ncenR14_tf,
                                   u8g2_font_ncenR12_tf, u8g2_font_ncenR10_tf};
  std::vector<Line> lines;
  int quoteH = 0;
  for (const uint8_t *f : fonts) {
    lines.clear();
    wrap(n.quote, f, 0, lines);
    quoteH = lines.size() * lineHeight(f);
    if (quoteH <= 140) break;
  }
  std::vector<Line> author;
  wrap(n.author, u8g2_font_ncenB10_tf, 0, author);
  int authorH = author.size() * lineHeight(u8g2_font_ncenB10_tf);
  int y = (200 - quoteH - 12 - authorH) / 2;

  beginDraw();
  do {
    display.fillScreen(GxEPD_WHITE);
    int cy = y;
    for (const Line &l : lines) {
      u8g2.setFont(l.font);
      cy += u8g2.getFontAscent();
      String s = forDisplay(l.text);
      u8g2.drawUTF8((200 - u8g2.getUTF8Width(s.c_str())) / 2, cy, s.c_str());
      cy += -u8g2.getFontDescent() + 3;
    }
    cy += 12;
    for (const Line &l : author) {
      u8g2.setFont(l.font);
      cy += u8g2.getFontAscent();
      String s = forDisplay(l.text);
      u8g2.drawUTF8(200 - MARGIN - u8g2.getUTF8Width(s.c_str()), cy, s.c_str());
      cy += -u8g2.getFontDescent() + 3;
    }
  } while (display.nextPage());
  drewThisSession = true;
}

void drawDetail(const std::vector<Line> &lines, int number, int total) {
  beginDraw();
  do {
    display.fillScreen(GxEPD_WHITE);
    int y = MARGIN;
    bool first = true;
    for (const Line &l : lines) {
      u8g2.setFont(l.font);
      y += (first ? 0 : l.gapBefore) + u8g2.getFontAscent();
      u8g2.drawUTF8(MARGIN, y, forDisplay(l.text).c_str());
      y += -u8g2.getFontDescent() + 3;
      first = false;
    }
    u8g2.setFont(u8g2_font_helvR08_tf);
    String num = String(number) + "/" + String(total);
    u8g2.drawUTF8(200 - MARGIN - u8g2.getUTF8Width(num.c_str()), 200 - 4, num.c_str());
  } while (display.nextPage());
  drewThisSession = true;
}

void showPage() {
  const Nugget &n = NUGGETS[shownDay];
  if (page == 0) {
    drawQuote(n);
    return;
  }
  auto pages = detailPages(n);
  if (page > (int)pages.size()) {
    page = 0;
    drawQuote(n);
    return;
  }
  drawDetail(pages[page - 1], page, pages.size());
}

void nextPage() {
  page++;
  showPage();
  Serial.printf("page %d\n", page);
}

// ---------- Power ----------

void powerOff() {
  Serial.println("power off");
  display.hibernate();
  digitalWrite(EPD_PWR, HIGH);
  digitalWrite(VBAT_PWR, LOW);  // cuts power when running on battery
  delay(500);
  // Still here, so we're on USB power: sleep until a button is pressed
  esp_sleep_enable_ext1_wakeup_io((1ULL << BTN_BOOT) | (1ULL << BTN_PWR), ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

void sleepUntilNext() {
  time_t now = time(nullptr);
  time_t wake = nextWake(now);
  uint64_t seconds = wake - now;
  if (page != 0 && seconds > BACK_TO_QUOTE_S) seconds = BACK_TO_QUOTE_S;

  display.hibernate();
  digitalWrite(EPD_PWR, HIGH);
  digitalWrite(AUDIO_PWR, HIGH);
  gpio_hold_en((gpio_num_t)EPD_PWR);
  gpio_hold_en((gpio_num_t)AUDIO_PWR);
  gpio_hold_en((gpio_num_t)VBAT_PWR);
  gpio_deep_sleep_hold_en();

  esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
  esp_sleep_enable_ext1_wakeup_io((1ULL << BTN_BOOT) | (1ULL << BTN_PWR), ESP_EXT1_WAKEUP_ANY_LOW);
  Serial.printf("sleeping %llu s\n", seconds);
  Serial.flush();
  esp_deep_sleep_start();
}

// ---------- Main ----------

bool pressed(int pin) { return digitalRead(pin) == LOW; }

void waitRelease(int pin) {
  uint32_t start = millis();
  while (pressed(pin)) {
    if (pin == BTN_PWR && millis() - start > POWER_OFF_HOLD_MS) powerOff();
    delay(10);
  }
}

void readSerialTime() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd == "R") {
    time_t now = time(nullptr);
    char stamp[32];
    strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&now));
    Serial.printf("now %s, RTC at boot: %s\n", stamp, rtcDiag.c_str());
  } else if (cmd.startsWith("T")) {
    time_t t = cmd.substring(1).toInt();
    if (t > 1700000000) {
      setSystemTime(t);
      rtcWrite(t);
      Serial.printf("clock: set to %ld\n", (long)t);
      int day = nuggetIndex(t);
      if (day != shownDay) {
        shownDay = day;
        page = 0;
        showPage();
      }
    }
  }
}

void setup() {
  gpio_hold_dis((gpio_num_t)EPD_PWR);
  gpio_hold_dis((gpio_num_t)VBAT_PWR);
  gpio_hold_dis((gpio_num_t)AUDIO_PWR);
  pinMode(VBAT_PWR, OUTPUT);
  digitalWrite(VBAT_PWR, HIGH);
  pinMode(EPD_PWR, OUTPUT);
  digitalWrite(EPD_PWR, LOW);
  pinMode(AUDIO_PWR, OUTPUT);
  digitalWrite(AUDIO_PWR, LOW);
  delay(10);
  pinMode(BTN_BOOT, INPUT);
  pinMode(BTN_PWR, INPUT);

  Serial.begin(115200);
  setenv("TZ", TZ_PARIS, 1);
  tzset();
  Wire.begin(I2C_SDA, I2C_SCL);
  syncClock();

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  display.init(0);
  u8g2.begin(display);
  u8g2.setFontMode(1);
  u8g2.setForegroundColor(GxEPD_BLACK);
  u8g2.setBackgroundColor(GxEPD_WHITE);

  time_t now = time(nullptr);
  int today = nuggetIndex(now);
  char stamp[32];
  strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&now));
  Serial.printf("philolog: %s, Jour %d, page %d\n", stamp, today + 1, page);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_EXT1) {
    uint64_t pins = esp_sleep_get_ext1_wakeup_status();
    int pin = (pins & (1ULL << BTN_PWR)) ? BTN_PWR : BTN_BOOT;
    waitRelease(pin);
    if (today != shownDay) {
      shownDay = today;
      page = 0;
      showPage();
    } else {
      nextPage();
    }
  } else if (today != shownDay || page != 0 || cause != ESP_SLEEP_WAKEUP_TIMER) {
    // New day, idle timeout on a detail page, or a fresh boot
    shownDay = today;
    page = 0;
    showPage();
  }
  // Otherwise the timer fired early: the right quote is already on screen.

  uint32_t lastActivity = millis();
  bool stayAwake = cause != ESP_SLEEP_WAKEUP_TIMER;
  while (stayAwake && millis() - lastActivity < AWAKE_MS) {
    readSerialTime();
    for (int pin : {BTN_BOOT, BTN_PWR}) {
      if (pressed(pin)) {
        delay(30);
        if (!pressed(pin)) continue;
        waitRelease(pin);
        nextPage();
        lastActivity = millis();
      }
    }
    delay(10);
  }
  sleepUntilNext();
}

void loop() {}
