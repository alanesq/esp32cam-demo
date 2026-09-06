/*

    As a test I gave ChatGPT my ESP32Cam demo sketch and asked it to re-write it but add features and 
    "take it to the next level".
    This is the result and it is so good I can't see any reason to try and write sketches myself any more!
    06Sep26




 * ESP32-CAM V2
 * AI-Thinker ESP32-CAM / OV2640
 *
 * Features:
 *  - PSRAM-aware camera setup, double buffering + CAMERA_GRAB_LATEST
 *  - Dedicated HTTP server (port 80) and MJPEG stream server (port 81)
 *  - Modern responsive dashboard embedded in the sketch
 *  - JSON status/settings API
 *  - Still capture to browser and microSD
 *  - Timelapse capture
 *  - Lightweight motion detection using frame luminance sampling
 *  - Motion event image capture + cooldown
 *  - NTP timestamps and sensible filenames
 *  - Persistent settings in Preferences/NVS
 *  - SD file listing, download and delete API
 *  - GPIO/flash control
 *  - Heap/PSRAM/RSSI/uptime diagnostics
 *  - Non-blocking Wi-Fi reconnect
 *
 * Notes:
 *  - Target: AI-Thinker ESP32-CAM with OV2640.
 *  - Set WIFI_SSID / WIFI_PASSWORD before flashing.
 *  - microSD is mounted in 1-bit mode so GPIO4 remains available for the flash LED.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_camera.h>
#include <FS.h>
#include <SD_MMC.h>
#include <Preferences.h>
#include <time.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <HTTPClient.h>
#include <Update.h>
#include <SPIFFS.h>

// ============================ USER CONFIG ============================
// Everything in this section is intended to be safe and easy to customise.
//
// The sketch first looks for a separate "wifiSettings.h" file.  That lets you
// keep Wi-Fi and OTA credentials out of the main sketch if you prefer.
// If that file is not present, the fallback values immediately below are used.
// =======================================================================

// if config file esists (wifiSettings.h) it gets the settings from there otherwise use the ones below

// __has_include() is a compile-time check.  It lets the sketch work both:
//   1. with a separate wifiSettings.h file, and
//   2. as a single self-contained .ino file.
//
// This is handy when moving the project between computers/installations.
#if __has_include("wifiSettings.h")            // if config file exists us it
  #include "wifiSettings.h"
#else                                          // if no config file found use these settings

  // wifi
    static const char *WIFI_SSID = "<WIFI SSID HERE>"
    static const char *WIFI_PASSWORD = "<WIFI PASSWORD HERE>"

  // sketch title
    static const char *HOSTNAME      = "esp32cam-v2";

  // OTA
    static const char *OTA_USERNAME = "admin";
    static const char *OTA_PASSWORD = "password";  

#endif

// Optional static hostname only; DHCP is used for IP configuration.
static const long GMT_OFFSET_SEC = 0;
static const int  DST_OFFSET_SEC = 3600;

// AI-Thinker ESP32-CAM camera pins.
//
// These GPIO numbers are part of the physical wiring between the ESP32 and
// the OV2640 camera module.  They are NOT general-purpose pins you can freely
// reassign without changing the camera wiring/board definition.
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define FLASH_GPIO_NUM     4

// ============================ LIMITS ============================
// Timing, GPIO and safety limits used throughout the program.
// Keeping these values in one place makes the sketch much easier to tune.
//
// A few names worth knowing:
//   *_MS       = a time interval in milliseconds
//   *_BYTES    = a size in bytes
//   *_GPIO_PIN = a physical ESP32 GPIO number
// =======================================================================
static const uint32_t WIFI_RETRY_MS       = 10000;
static const uint32_t MOTION_SAMPLE_MS    = 700;
static const uint32_t MOTION_COOLDOWN_MS  = 10000;
static const uint32_t TIMELAPSE_MIN_MS    = 1000;
static const uint32_t SD_MIN_FREE_BYTES   = 2UL * 1024UL * 1024UL;
static const float MAX_TEMP_C = 75.0f;
static const int INPUT_GPIO_PIN = 12;
static const int OUTPUT_GPIO_PIN = 13;
static const int INDICATOR_LED_PIN = 33;
static const bool OUTPUT_ON_LEVEL = LOW;
static const uint32_t OUTPUT_TIME_LIMIT_MS = 120UL * 60UL * 1000UL;
static const uint8_t RGB_SAMPLE_BYTES = 60;
#define ENABLE_OTA 1

// ============================ SETTINGS ============================
// "Settings" contains the user-adjustable camera behaviour.
//
// This is deliberately kept separate from the runtime variables below.
// The Settings structure is stored in ESP32 Preferences (NVS), so the last
// selected values survive a reboot/power cycle.

struct Settings {
  uint8_t  framesize = FRAMESIZE_VGA;
  uint8_t  quality = 10;
  int8_t   brightness = 0;
  int8_t   contrast = 0;
  int8_t   saturation = 0;
  uint16_t exposure = 0;
  uint8_t  gain = 0;
  bool     hmirror = false;
  bool     vflip = false;
  bool     flash = false;
  bool     motion = false;
  uint16_t motionThreshold = 13;
  uint16_t motionMinChanged = 8;
  uint32_t timelapseMs = 0;
};

// One global Settings object holds the current camera configuration.
Settings settings;

// Preferences gives us non-volatile storage backed by the ESP32's NVS area.
// It is similar in purpose to EEPROM, but is key/value based.
Preferences prefs;

// Port 80: normal HTTP requests for the dashboard, APIs, captures, etc.
// Port 81: a separate raw MJPEG server used for the live camera stream.
//
// Keeping the stream on its own server prevents the normal web/API server
// from having to manage a long-lived multipart image response.
WebServer server(80);
WiFiServer streamServer(81);

uint32_t captureCount = 0;
uint32_t motionCount = 0;
uint32_t timelapseCount = 0;
uint32_t jpegBytesLast = 0;
uint32_t captureTimeUsLast = 0;

bool sdReady = false;
bool spiffsReady = false;
int imageCounter = 0;
uint32_t outputChangedAt = 0;
uint8_t illuminationBrightness = 0;
uint32_t nextTimelapse = 0;
uint32_t lastMotionSample = 0;
uint32_t lastMotionEvent = 0;
uint32_t motionDisplayUntil = 0;
bool motionDetected = false;
uint32_t lastWiFiAttempt = 0;
uint32_t streamFrameCounter = 0;

// ============================ HELPERS ============================
// Small utility functions live here.  The aim is to keep the HTTP handlers
// and the main loop readable instead of putting lots of low-level conversion,
// filename and settings code inline.

// Escape characters that have special meaning inside a JSON string.
// We build JSON by hand in this sketch, so strings must be escaped before
// they are inserted into JSON responses.
String jsonEscape(const String &s) {
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '"') o += '\\';
    if (c == '\\') o += '\\';
    if (c == '\n') { o += "\\n"; continue; }
    if (c == '\r') { o += "\\r"; continue; }
    o += c;
  }
  return o;
}

// Convert the camera driver's numeric frame-size constant into the same
// human-readable name shown in the web dashboard.
const char *framesizeName(uint8_t f) {
  switch (f) {
    case FRAMESIZE_QQVGA: return "QQVGA";
    case FRAMESIZE_QVGA:  return "QVGA";
    case FRAMESIZE_VGA:   return "VGA";
    case FRAMESIZE_SVGA:  return "SVGA";
    case FRAMESIZE_XGA:   return "XGA";
    case FRAMESIZE_SXGA:  return "SXGA";
    case FRAMESIZE_UXGA:  return "UXGA";
    default: return "UNKNOWN";
  }
}

// Do the reverse of framesizeName(): turn a dashboard string such as "VGA"
// back into the numeric frame-size constant expected by the camera driver.
// If the supplied text is unknown, keep the current value unchanged.
uint8_t parseFramesize(const String &s) {
  if (s == "QQVGA") return FRAMESIZE_QQVGA;
  if (s == "QVGA")  return FRAMESIZE_QVGA;
  if (s == "VGA")   return FRAMESIZE_VGA;
  if (s == "SVGA")  return FRAMESIZE_SVGA;
  if (s == "XGA")   return FRAMESIZE_XGA;
  if (s == "SXGA")  return FRAMESIZE_SXGA;
  if (s == "UXGA")  return FRAMESIZE_UXGA;
  return settings.framesize;
}

// Read a boolean HTTP argument.  A missing argument leaves the current
// setting alone, while common true/false spellings are accepted.
bool argBool(const String &name, bool current) {
  if (!server.hasArg(name)) return current;
  String v = server.arg(name);
  v.toLowerCase();
  return v == "1" || v == "true" || v == "on" || v == "yes";
}

// Safety helper: force a value into an allowed range.
// Example: clampInt(99, 0, 31) returns 31.
int clampInt(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Return a filesystem-friendly UTC/local time string for filenames.
// If NTP has not supplied a valid time yet, a fallback timestamp is used
// instead of producing a nonsense date.
String timestampString() {
  struct tm tmNow;
  if (!getLocalTime(&tmNow, 20)) return String(millis());
  char b[24];
  strftime(b, sizeof(b), "%Y%m%d_%H%M%S", &tmNow);
  return String(b);
}

// Generate a unique path for a JPEG on the SD card.
// The filename includes a prefix (IMG, MOT, TLP, etc.), the timestamp and
// an incrementing counter so repeated captures do not overwrite one another.
String uniquePhotoPath(const char *prefix = "IMG") {
  String base = "/" + String(prefix) + "_" + timestampString();
  String path = base + ".jpg";
  uint16_t n = 1;
  while (sdReady && SD_MMC.exists(path) && n < 1000) {
    path = base + "_" + String(n++) + ".jpg";
  }
  return path;
}

// Look through existing files at startup and recover the highest numbered
// image counter.  This prevents the counter from jumping backwards after a
// reboot and accidentally generating duplicate names.
void scanNumberedImages() {
  imageCounter = 0;
  if (!sdReady) return;
  File root = SD_MMC.open("/img");
  if (!root || !root.isDirectory()) { if (root) root.close(); return; }
  File f = root.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      String n = String(f.name());
      if (n.startsWith("/img/") && n.endsWith(".jpg")) {
        int a = 5;
        int b = n.length() - 4;
        String num = n.substring(a, b);
        bool numeric = num.length() > 0;
        for (size_t i = 0; i < num.length(); ++i) if (!isDigit(num[i])) numeric = false;
        if (numeric) { int n = (int)num.toInt(); if (n > imageCounter) imageCounter = n; }
      }
    }
    f.close();
    f = root.openNextFile();
  }
  root.close();
}

// Build a simple sequential filename/path for image storage.
String numberedPhotoPath() {
  if (!sdReady) return String("/img/1.jpg");
  int n = imageCounter + 1;
  while (n < 100000 && SD_MMC.exists("/img/" + String(n) + ".jpg")) ++n;
  imageCounter = n;
  return "/img/" + String(n) + ".jpg";
}

// Turn the ESP32-CAM's built-in flash LED on or off.
// The actual pin level depends on the board wiring, so the polarity is
// kept in this one small function rather than scattered through the code.
void setFlash(bool on) {
  settings.flash = on;
  digitalWrite(FLASH_GPIO_NUM, on ? HIGH : LOW);
}

// Save every user-adjustable setting to NVS (non-volatile storage).
// Preferences stores simple values by key, so the next boot can restore them.
void saveSettings() {
  prefs.begin("camv2", false);
  prefs.putUChar("size", settings.framesize);
  prefs.putUChar("qual", settings.quality);
  prefs.putChar("bright", settings.brightness);
  prefs.putChar("contr", settings.contrast);
  prefs.putChar("sat", settings.saturation);
  prefs.putUShort("exp", settings.exposure);
  prefs.putUChar("gain", settings.gain);
  prefs.putBool("hm", settings.hmirror);
  prefs.putBool("vf", settings.vflip);
  prefs.putBool("flash", settings.flash);
  prefs.putBool("motion", settings.motion);
  prefs.putUShort("mth", settings.motionThreshold);
  prefs.putUShort("mmc", settings.motionMinChanged);
  prefs.putULong("tlms", settings.timelapseMs);
  prefs.end();
}

// Load settings saved by saveSettings().
// Missing keys are given the defaults from the Settings structure.
void loadSettings() {
  prefs.begin("camv2", true);
  settings.framesize = prefs.getUChar("size", FRAMESIZE_VGA);
  settings.quality = prefs.getUChar("qual", 10);
  settings.brightness = prefs.getChar("bright", 0);
  settings.contrast = prefs.getChar("contr", 0);
  settings.saturation = prefs.getChar("sat", 0);
  settings.exposure = prefs.getUShort("exp", 0);
  settings.gain = prefs.getUChar("gain", 0);
  settings.hmirror = prefs.getBool("hm", false);
  settings.vflip = prefs.getBool("vf", false);
  settings.flash = prefs.getBool("flash", false);
  settings.motion = prefs.getBool("motion", false);
  settings.motionThreshold = prefs.getUShort("mth", 13);
  settings.motionMinChanged = prefs.getUShort("mmc", 8);
  settings.timelapseMs = prefs.getULong("tlms", 0);
  prefs.end();

  settings.quality = clampInt(settings.quality, 5, 63);
  settings.brightness = clampInt(settings.brightness, -2, 2);
  settings.contrast = clampInt(settings.contrast, -2, 2);
  settings.saturation = clampInt(settings.saturation, -2, 2);
  settings.exposure = clampInt(settings.exposure, 0, 1200);
  settings.gain = clampInt(settings.gain, 0, 31);
  settings.motionThreshold = clampInt(settings.motionThreshold, 1, 60);
  settings.motionMinChanged = clampInt(settings.motionMinChanged, 1, 64);
}

// Push our Settings structure into the OV2640 sensor.
//
// This function is important because changing a value in RAM does not by
// itself change the camera.  The sensor object must also receive the new
// brightness/contrast/exposure/etc. values.
void applySensorSettings() {
  // The camera driver gives us the live sensor object.  All image controls
  // below are applied directly to that sensor.
  sensor_t *s = esp_camera_sensor_get();
  if (!s) return;
  s->set_framesize(s, (framesize_t)settings.framesize);
  s->set_quality(s, settings.quality);
  s->set_brightness(s, settings.brightness);
  s->set_contrast(s, settings.contrast);
  s->set_saturation(s, settings.saturation);
  s->set_hmirror(s, settings.hmirror ? 1 : 0);
  s->set_vflip(s, settings.vflip ? 1 : 0);
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  if (settings.exposure == 0 && settings.gain == 0) {
    s->set_exposure_ctrl(s, 1);
    s->set_gain_ctrl(s, 1);
  } else {
    s->set_exposure_ctrl(s, 0);
    s->set_gain_ctrl(s, 0);
    s->set_agc_gain(s, settings.gain);
    s->set_aec_value(s, settings.exposure);
  }
}

// ============================ CAMERA ============================
// Initialise the OV2640 camera.
//
// This constructs the camera configuration structure expected by the ESP32
// camera driver, enables PSRAM-aware frame buffering when available, starts
// the driver and finally applies the stored sensor settings.
bool initCamera() {
  // camera_config_t is the driver's description of how the ESP32 is wired
  // to the OV2640 and how captured frames should be buffered/encoded.
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  // JPEG is used because it keeps frame data compact enough for Wi-Fi and
  // SD storage.  It also lets the browser display the frame directly.
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = (framesize_t)settings.framesize;
  config.jpeg_quality = settings.quality;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_count = psramFound() ? 2 : 1;

  if (!psramFound()) {
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.frame_size = (framesize_t)min((int)settings.framesize, (int)FRAMESIZE_VGA);
  }

  // Hand the completed configuration to the Espressif camera driver.
  esp_err_t err = esp_camera_init(&config);
  // Any non-OK result means the camera driver could not be started.
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  applySensorSettings();
  return true;
}

// Capture a frame from the camera and return ownership of the frame buffer
// to the caller.  The caller MUST eventually call esp_camera_fb_return()
// or the camera can run out of frame buffers.
camera_fb_t *captureFrame() {
  uint32_t start = micros();
  camera_fb_t *fb = esp_camera_fb_get();
  captureTimeUsLast = micros() - start;
  if (fb) {
    captureCount++;
    jpegBytesLast = fb->len;
  }
  return fb;
}

// Write one already-captured camera frame to a filesystem path.
// "fb" is the camera frame buffer returned by esp_camera_fb_get().
bool saveFrame(camera_fb_t *fb, const String &path) {
  if (!sdReady || !fb) return false;
  if (SD_MMC.totalBytes() - SD_MMC.usedBytes() < SD_MIN_FREE_BYTES) return false;
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) return false;
  size_t written = f.write(fb->buf, fb->len);
  f.close();
  return written == fb->len;
}


// Fallback storage helper for a frame when the SD card is unavailable.
// The sketch mainly uses SD for photo storage, but SPIFFS can still provide
// a small internal fallback for situations where that is useful.
bool saveFrameSPIFFS(camera_fb_t *fb) {
  if (!spiffsReady || !fb) return false;
  SPIFFS.remove("/image.jpg");
  File f = SPIFFS.open("/image.jpg", FILE_WRITE);
  if (!f) return false;
  size_t written = f.write(fb->buf, fb->len);
  f.close();
  return written == fb->len;
}

// Capture one fresh JPEG and optionally save it to SD.
//
// This is the common capture path used by normal snapshots, motion events
// and timelapse images.  The function also updates capture statistics.
bool captureToSD(const char *prefix, String *savedPath = nullptr) {
  // A capture can request the normal flash setting or a non-zero
  // illumination level from the dashboard.
  bool flashOn = settings.flash || illuminationBrightness > 0;
  uint8_t oldBrightness = illuminationBrightness;
  if (flashOn) {
    analogWrite(FLASH_GPIO_NUM, settings.flash ? 255 : illuminationBrightness);
    delay(100);
  }

  camera_fb_t *fb = captureFrame();
  if (!fb) {
    analogWrite(FLASH_GPIO_NUM, oldBrightness);
    return false;
  }

  String path;
  bool ok = false;
  if (sdReady) {
    path = uniquePhotoPath(prefix);
    ok = saveFrame(fb, path);
  } else if (spiffsReady) {
    path = "/image.jpg";
    ok = saveFrameSPIFFS(fb);
  }
  esp_camera_fb_return(fb);
  if (flashOn) analogWrite(FLASH_GPIO_NUM, oldBrightness);
  if (ok && savedPath) *savedPath = path;
  return ok;
}


// ============================ SD ============================
// Mount the microSD card in 1-bit mode.
//
// 1-bit mode matters on this board because it reduces SD_MMC pin usage and
// leaves GPIO4 available for the built-in flash LED.
bool initSD() {
  // AI-Thinker ESP32-CAM: 1-bit mode keeps GPIO4 available for flash LED.
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD_MMC mount failed");
    return false;
  }
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) return false;
  Serial.printf("SD ready: %.2f MB\n", SD_MMC.cardSize() / 1048576.0f);
  SD_MMC.mkdir("/img");
  scanNumberedImages();
  return true;
}

// Remove old photos when the card is running low on free space.
// The goal is to prevent future captures from failing simply because the
// filesystem has filled up.  "keepAtLeast" prevents the cleanup from trying
// to remove the only remaining image.
void pruneOldestPhotos(uint8_t keepAtLeast = 1) {
  if (!sdReady) return;
  // Keep pruning until at least 5% free or 2MB free, whichever is larger.
  const uint64_t total = SD_MMC.totalBytes();
  const uint64_t freeB = total > SD_MMC.usedBytes() ? total - SD_MMC.usedBytes() : 0;
  if (freeB > max<uint64_t>(SD_MIN_FREE_BYTES, total / 20)) return;

  File root = SD_MMC.open("/");
  if (!root || !root.isDirectory()) return;

  String oldest;
  File file = root.openNextFile();
  while (file) {
    String n = file.name();
    if (!file.isDirectory() && n.endsWith(".jpg")) {
      if (oldest.isEmpty() || n < oldest) oldest = n;
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();

  if (!oldest.isEmpty()) {
    SD_MMC.remove(oldest);
  }
}

// ============================ JSON / API ============================
// Build the JSON object returned by /api/status.
// The browser periodically polls this endpoint to refresh diagnostics,
 // GPIO states and motion status without reloading the whole page.
String statusJson() {
  uint64_t total = sdReady ? SD_MMC.totalBytes() : 0;
  uint64_t used  = sdReady ? SD_MMC.usedBytes() : 0;
  uint64_t freeB = total > used ? total - used : 0;

  String j = "{";
  j += "\"hostname\":\"" + jsonEscape(String(HOSTNAME)) + "\",";
  j += "\"ip\":\"" + jsonEscape(WiFi.localIP().toString()) + "\",";
  j += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  j += "\"wifi\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  j += "\"uptime\":" + String(millis() / 1000UL) + ",";
  j += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  j += "\"freePSRAM\":" + String(psramFound() ? ESP.getFreePsram() : 0) + ",";
  j += "\"sdReady\":" + String(sdReady ? "true" : "false") + ",";
  j += "\"sdFree\":" + String((unsigned long long)freeB) + ",";
  j += "\"captures\":" + String(captureCount) + ",";
  j += "\"motionEvents\":" + String(motionCount) + ",";
  j += "\"motionDetected\":" + String(motionDetected ? "true" : "false") + ",";
  j += "\"timelapse\":" + String(timelapseCount) + ",";
  j += "\"lastJpegBytes\":" + String(jpegBytesLast) + ",";
  j += "\"lastCaptureMs\":" + String(captureTimeUsLast / 1000UL) + ",";
  j += "\"resolution\":\"" + String(framesizeName(settings.framesize)) + "\",";
  j += "\"quality\":" + String(settings.quality) + ",";
  j += "\"motionEnabled\":" + String(settings.motion ? "true" : "false") + ",";
  j += "\"timelapseMs\":" + String(settings.timelapseMs) + ",";
  j += "\"gpioInputPin\":" + String(INPUT_GPIO_PIN) + ",";
  j += "\"gpioInputState\":" + String(digitalRead(INPUT_GPIO_PIN) ? "true" : "false") + ",";
  j += "\"gpioOutputPin\":" + String(OUTPUT_GPIO_PIN) + ",";
  j += "\"gpioOutputState\":" + String(digitalRead(OUTPUT_GPIO_PIN) == OUTPUT_ON_LEVEL ? "true" : "false");
  j += "}";
  return j;
}

// Build a JSON representation of the current Settings structure.
// The dashboard uses this both when initially loading the form and after
// applying new values.
String settingsJson() {
  String j = "{";
  j += "\"resolution\":\"" + String(framesizeName(settings.framesize)) + "\",";
  j += "\"quality\":" + String(settings.quality) + ",";
  j += "\"brightness\":" + String(settings.brightness) + ",";
  j += "\"contrast\":" + String(settings.contrast) + ",";
  j += "\"saturation\":" + String(settings.saturation) + ",";
  j += "\"exposure\":" + String(settings.exposure) + ",";
  j += "\"gain\":" + String(settings.gain) + ",";
  j += "\"hmirror\":" + String(settings.hmirror ? "true" : "false") + ",";
  j += "\"vflip\":" + String(settings.vflip ? "true" : "false") + ",";
  j += "\"flash\":" + String(settings.flash ? "true" : "false") + ",";
  j += "\"motion\":" + String(settings.motion ? "true" : "false") + ",";
  j += "\"motionThreshold\":" + String(settings.motionThreshold) + ",";
  j += "\"motionMinChanged\":" + String(settings.motionMinChanged) + ",";
  j += "\"timelapseMs\":" + String(settings.timelapseMs) + ",";
  j += "\"gpioInputPin\":" + String(INPUT_GPIO_PIN) + ",";
  j += "\"gpioInputState\":" + String(digitalRead(INPUT_GPIO_PIN) ? "true" : "false") + ",";
  j += "\"gpioOutputPin\":" + String(OUTPUT_GPIO_PIN) + ",";
  j += "\"gpioOutputState\":" + String(digitalRead(OUTPUT_GPIO_PIN) == OUTPUT_ON_LEVEL ? "true" : "false");
  j += "}";
  return j;
}

// Common helper for sending JSON with the correct content type.
void sendJson(const String &body, int code = 200) {
  server.send(code, "application/json; charset=utf-8", body);
}

void handleStatus() { sendJson(statusJson()); }
void handleSettingsGet() { sendJson(settingsJson()); }

// Read camera/settings values from HTTP query/form arguments and validate
// them before copying them into the global Settings structure.
void applyArgsToSettings() {
  if (server.hasArg("resolution")) settings.framesize = parseFramesize(server.arg("resolution"));
  if (server.hasArg("quality")) settings.quality = clampInt(server.arg("quality").toInt(), 5, 63);
  if (server.hasArg("brightness")) settings.brightness = clampInt(server.arg("brightness").toInt(), -2, 2);
  if (server.hasArg("contrast")) settings.contrast = clampInt(server.arg("contrast").toInt(), -2, 2);
  if (server.hasArg("saturation")) settings.saturation = clampInt(server.arg("saturation").toInt(), -2, 2);
  if (server.hasArg("exposure")) settings.exposure = clampInt(server.arg("exposure").toInt(), 0, 1200);
  if (server.hasArg("gain")) settings.gain = clampInt(server.arg("gain").toInt(), 0, 31);
  settings.hmirror = argBool("hmirror", settings.hmirror);
  settings.vflip = argBool("vflip", settings.vflip);
  settings.flash = argBool("flash", settings.flash);
  settings.motion = argBool("motion", settings.motion);
  if (server.hasArg("motionThreshold")) settings.motionThreshold = clampInt(server.arg("motionThreshold").toInt(), 1, 60);
  if (server.hasArg("motionMinChanged")) settings.motionMinChanged = clampInt(server.arg("motionMinChanged").toInt(), 1, 64);
  if (server.hasArg("timelapseMs")) {
    long v = server.arg("timelapseMs").toInt();
    settings.timelapseMs = v <= 0 ? 0 : max<long>(TIMELAPSE_MIN_MS, v);
  }

  applySensorSettings();
  setFlash(settings.flash);
  nextTimelapse = millis() + (settings.timelapseMs ? settings.timelapseMs : 0);
}

// Legacy/general settings handler: apply incoming HTTP arguments,
// save the new settings to NVS, then return the updated settings as JSON.
void handleSettingsSet() {
  applyArgsToSettings();
  saveSettings();
  sendJson(settingsJson());
}

// POST version of the settings handler used by the modern dashboard.
// POST is preferable for a larger group of form values because the values
// are sent in the request body rather than encoded into the URL.
void handleSettingsSetPost() {
  applyArgsToSettings();
  saveSettings();
  sendJson(settingsJson());
}

// Send one camera frame directly as an HTTP JPEG response.
// This is the core helper behind the /jpg, /jpeg, /photo and related endpoints.
void sendJpegFrame(camera_fb_t *fb) {
  WiFiClient client = server.client();
  client.setTimeout(1000);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  client.write(fb->buf, fb->len);
}

// HTTP handler for /jpg: capture a fresh still image and return it directly.
void handleJPG() {
  static uint32_t lastJpg = 0;
  if (millis() - lastJpg > 3000) {
    camera_fb_t *drop = esp_camera_fb_get();
    if (drop) esp_camera_fb_return(drop);
    lastJpg = millis();
  }
  camera_fb_t *fb = nullptr;
  for (int i = 0; i < 3 && !fb; ++i) {
    fb = captureFrame();
    if (!fb) delay(30);
  }
  if (!fb) return server.send(503, "text/plain", "Camera capture failed");
  sendJpegFrame(fb);
  esp_camera_fb_return(fb);
}

// HTTP handler for the dashboard's "Capture" action.
// A query parameter can request that the image is also stored on SD.
void handleCapture() {
  bool flash = settings.flash;
  if (server.hasArg("flash")) flash = argBool("flash", settings.flash);
  bool oldFlash = settings.flash;
  if (flash) analogWrite(FLASH_GPIO_NUM, 255);
  delay(flash ? 100 : 0);
  camera_fb_t *fb = captureFrame();
  if (flash) analogWrite(FLASH_GPIO_NUM, illuminationBrightness);
  settings.flash = oldFlash;
  if (!fb) return server.send(503, "text/plain", "Camera capture failed");
  if (!server.hasArg("save") || server.arg("save") != "0") {
    if (sdReady) saveFrame(fb, uniquePhotoPath("IMG"));
    else if (spiffsReady) saveFrameSPIFFS(fb);
  }
  sendJpegFrame(fb);
  esp_camera_fb_return(fb);
}

// /photo is another still-image endpoint kept for compatibility with
// applications that expect this path.
void handlePhoto() {
  bool flashOn = settings.flash || illuminationBrightness > 0;
  uint8_t old = illuminationBrightness;
  if (flashOn) { analogWrite(FLASH_GPIO_NUM, settings.flash ? 255 : illuminationBrightness); delay(100); }
  camera_fb_t *fb = captureFrame();
  if (flashOn) analogWrite(FLASH_GPIO_NUM, old);
  if (!fb) return server.send(503, "text/plain", "Camera capture failed");
  String path;
  bool ok;
  if (sdReady) { path = numberedPhotoPath(); ok = saveFrame(fb, path); }
  else { path = "/image.jpg"; ok = saveFrameSPIFFS(fb); }
  esp_camera_fb_return(fb);
  if (!ok) { if(sdReady && imageCounter>0) imageCounter--; return server.send(503, "text/plain", "Failed to save image"); }
  server.send(200, "text/plain", "Image saved: " + path);
}


// /img is another compatibility endpoint.  Its optional query argument is
// accepted so existing callers can continue using /img?img=1.
void handleImg() {
  if (sdReady) {
    int n = imageCounter;
    if (server.hasArg("img")) {
      String a = server.arg("img");
      if (a.length() == 0) return server.send(400, "text/plain", "Invalid img number");
      n = a.toInt();
      if (n < 1) return server.send(400, "text/plain", "Invalid img number");
    }
    if (n < 1) return server.send(404, "text/plain", "No numbered images");

    String path = "/img/" + String(n) + ".jpg";
    File f = SD_MMC.open(path, FILE_READ);
    if (!f || f.isDirectory()) {
      if (f) f.close();
      return server.send(404, "text/plain", "Image not found: " + path);
    }
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.streamFile(f, "image/jpeg");
    f.close();
  } else if (spiffsReady) {
    File f = SPIFFS.open("/image.jpg", FILE_READ);
    if (!f) return server.send(404, "text/plain", "No image found");
    server.streamFile(f, "image/jpeg");
    f.close();
  } else {
    server.send(404, "text/plain", "No storage available");
  }
}

// /jpeg is an alias for a normal JPEG still capture.
void handleJpeg() {
  server.send(200, "text/html", "<!doctype html><html><body><img id='i' src='/jpg'><script>setInterval(()=>{i.src='/jpg?t='+Date.now()},2000)</script></body></html>");
}


// Return a JSON list of stored image files on the SD card.
void handleFiles() {
  if (!sdReady) return sendJson("{\"error\":\"SD not available\"}", 503);
  String j = "[";
  File root = SD_MMC.open("/");
  File f = root.openNextFile();
  bool first = true;
  while (f) {
    if (!f.isDirectory() && String(f.name()).endsWith(".jpg")) {
      if (!first) j += ",";
      j += "{\"name\":\"" + jsonEscape(String(f.name())) + "\",\"size\":" + String((unsigned long)f.size()) + "}";
      first = false;
    }
    f.close();
    f = root.openNextFile();
  }
  root.close();
  j += "]";
  sendJson(j);
}

// Normalise/validate a requested filesystem path before opening it.
// This is important because the path ultimately comes from an HTTP client.
String safePath(String p) {
  if (!p.startsWith("/")) p = "/" + p;
  p.replace("..", "");
  return p;
}

// Send an image file from the SD card back to the browser.
// The filename is supplied as a request parameter and is sanitised first.
void handleDownload() {
  if (!sdReady || !server.hasArg("name")) return server.send(400, "text/plain", "Missing name");
  String p = safePath(server.arg("name"));
  File f = SD_MMC.open(p, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    return server.send(404, "text/plain", "Not found");
  }
  server.streamFile(f, "image/jpeg");
  f.close();
}

void handleDelete() {
  if (!sdReady || !server.hasArg("name")) return server.send(400, "text/plain", "Missing name");
  String p = safePath(server.arg("name"));
  bool ok = SD_MMC.remove(p);
  sendJson(String("{\"ok\":") + (ok ? "true" : "false") + "}", ok ? 200 : 404);
}

// Simple extension check used when deciding which files count as photos.
bool isJpegName(const String &name) {
  String n = name;
  n.toLowerCase();
  return n.endsWith(".jpg") || n.endsWith(".jpeg");
}

uint32_t clearJpegFiles(fs::FS &fs, const char *rootPath) {
  uint32_t removed = 0;
  File root = fs.open(rootPath);
  if (!root || !root.isDirectory()) { if (root) root.close(); return 0; }
  File f = root.openNextFile();
  while (f) {
    String n = f.name();
    bool dir = f.isDirectory();
    f.close();
    if (dir) {
      if (n == "/img") {
        File sub = fs.open(n);
        if (sub && sub.isDirectory()) {
          File sf = sub.openNextFile();
          while (sf) {
            String sn = sf.name();
            bool sdir = sf.isDirectory();
            sf.close();
            if (!sdir && isJpegName(sn) && fs.remove(sn)) ++removed;
            sf = sub.openNextFile();
          }
          sub.close();
        }
      }
    } else if (isJpegName(n) && fs.remove(n)) {
      ++removed;
    }
    f = root.openNextFile();
  }
  root.close();
  return removed;
}

// HTTP handler that deletes all stored JPEG images.
// This is intentionally separate from handleDelete(), which removes one file.
void handleClearImages() {
  uint32_t removed = 0;
  if (sdReady) removed = clearJpegFiles(SD_MMC, "/");
  if (spiffsReady && SPIFFS.exists("/image.jpg")) { if (SPIFFS.remove("/image.jpg")) ++removed; }
  scanNumberedImages();
  sendJson(String("{\"ok\":true,\"removed\":") + String(removed) + "}");
}

// Return raw image/camera data used by one of the compatibility endpoints.
void handleData() {
  uint64_t freeB = sdReady ? (SD_MMC.totalBytes() - SD_MMC.usedBytes()) : 0;
  String out;
  if (!sdReady) out += "NO SD CARD DETECTED";
  else out += "SD Card: " + String(SD_MMC.usedBytes()/1048576ULL) + "MB used - " + String(freeB/1048576ULL) + "MB free";
  out += ",Illumination led brightness=" + String(illuminationBrightness) + " &ensp; Flash is " + String(settings.flash ? "Enabled" : "Off");
  out += ",Current time: " + timestampString();
  out += ",GPIO output pin " + String(OUTPUT_GPIO_PIN) + " is: " + String(digitalRead(OUTPUT_GPIO_PIN)==OUTPUT_ON_LEVEL ? "ON" : "OFF");
  out += " &ensp; GPIO input " + String(INPUT_GPIO_PIN) + " is: " + String(digitalRead(INPUT_GPIO_PIN) ? "ON" : "OFF");
  out += ",Image size: " + String(framesizeName(settings.framesize));
  out += ",Free memory: " + String(ESP.getFreeHeap()/1000) + "K &ensp; Wifi strength: " + String(WiFi.RSSI()) + "dBm &ensp; Temperature: " + String(temperatureRead()) + "C";
  server.send(200, "text/plain", out);
}

// Control the spare output GPIO (GPIO13).
// The output is active-low on this particular build, so OUTPUT_ON_LEVEL
// defines which digitalWrite() value means "ON".
void handleSwitch() {
  if (!server.hasArg("on")) return server.send(400, "text/plain", "error - no command received");
  int v = server.arg("on").toInt();
  if (v == 0) { digitalWrite(OUTPUT_GPIO_PIN, !OUTPUT_ON_LEVEL); outputChangedAt = millis(); server.send(200,"text/plain","Switched off"); }
  else if (v == 1) { digitalWrite(OUTPUT_GPIO_PIN, OUTPUT_ON_LEVEL); outputChangedAt = millis(); server.send(200,"text/plain","Switched on"); }
  else server.send(400,"text/plain","Invalid value");
}

// Tiny health-check endpoint: useful for external scripts because it returns
// a predictable response without requiring a camera capture.
void handlePing() { server.send(200, "text/plain", "ok"); }

// Explicitly reboot the board.  The short delay lets the HTTP response leave
// the network stack before the ESP32 resets.
void handleReboot() { server.send(200, "text/plain", "Rebooting...."); delay(300); ESP.restart(); }

// Generic 404 handler.  It prints the requested URI, method and arguments,
// which makes debugging accidental/wrong URLs much easier from a browser
// or Serial-connected development session.
void handleNotFound() {
  String out = "File Not Found\n\nURI: " + server.uri() + "\nMethod: " + String(server.method()==HTTP_GET ? "GET" : "POST") + "\nArguments: " + String(server.args()) + "\n";
  for (uint8_t i=0;i<server.args();i++) out += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  server.send(404, "text/plain", out);
}

// Small outbound HTTP helper used by diagnostic/testing functionality.
// It performs a GET and optionally returns both the page body and the
// requested URL/response information to the caller.
int requestWebPage(String *page, String *received, int maxWaitTime) {
  if (!page || !received) return -1;
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(maxWaitTime);
  if (!http.begin(client, *page)) { *received = "error: begin"; return -1; }
  int code = http.GET();
  *received = code > 0 ? http.getString() : "error:" + String(code);
  http.end();
  return code;
}

// Small diagnostic endpoint used to check that the HTTP server and a
// camera capture path are working.
void handleTest() {
  // Do not temporarily change GPIO13 to INPUT here.  That makes the displayed
  // state misleading and can also interfere with attached hardware.
  const int inRaw = digitalRead(INPUT_GPIO_PIN);
  const int outRaw = digitalRead(OUTPUT_GPIO_PIN);
  const bool outOn = (outRaw == OUTPUT_ON_LEVEL);

  String html = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>ESP32-CAM V2 Test</title>";
  html += "<style>body{font-family:Arial,sans-serif;margin:20px;line-height:1.55}.box{max-width:720px;padding:16px;border:1px solid #ccc;border-radius:10px}button{padding:10px 14px;margin:4px}code{background:#f3f3f3;padding:2px 5px;border-radius:4px}.hostnameBadge{font-size:0.55em;font-weight:600;opacity:.8;padding:.25em .55em;border-radius:.5em;background:#e8eef5;vertical-align:middle}</style></head><body>";
  html += "<div class='box'><h2>ESP32-CAM V2 test / diagnostics</h2>";
  html += "Temperature: <b>" + String(temperatureRead()) + " C</b><br>";
  html += "Free heap: <b>" + String(ESP.getFreeHeap()) + " bytes</b><br>";
  html += "Free PSRAM: <b>" + String(psramFound()?ESP.getFreePsram():0) + " bytes</b><br><hr>";
  html += "GPIO12 input: <b>" + String(inRaw ? "HIGH (1)" : "LOW (0)") + "</b>";
  html += "<br>Mode: INPUT_PULLUP (LOW normally means the input is being pulled to ground).<br><br>";
  html += "GPIO13 output: <b>" + String(outOn ? "ON" : "OFF") + "</b>";
  html += " &nbsp; raw level: <b>" + String(outRaw ? "HIGH (1)" : "LOW (0)") + "</b>";
  html += "<br>Logical ON level is <code>" + String(OUTPUT_ON_LEVEL ? "HIGH" : "LOW") + "</code> (active-low output).<br>";
  html += "<button onclick=\"location.href=\'/switch?on=1\'\">Output ON</button>";
  html += "<button onclick=\"location.href=\'/switch?on=0\'\">Output OFF</button>";
  html += "<hr><a href='/'>Return to dashboard</a></div></body></html>";
  server.send(200,"text/html",html);
}

// Capture a JPEG and provide sampled RGB-related data for diagnostics.
// This is not intended to be a full JPEG decoder; it is a lightweight
// test/inspection endpoint.
void readRGBImage() {
  if (!psramFound()) return server.send(503,"text/plain","error: no psram available");
  camera_fb_t *fb = captureFrame();
  if (!fb) return server.send(503,"text/plain","error: failed to capture image");
  size_t bytes = (size_t)fb->width * fb->height * 3;
  uint8_t *rgb = (uint8_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  if (!rgb) { esp_camera_fb_return(fb); return server.send(503,"text/plain","error: not enough free psram"); }
  bool ok = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb);
  if (!ok) { heap_caps_free(rgb); esp_camera_fb_return(fb); return server.send(500,"text/plain","error: failed to convert image to RGB data"); }
  if (server.hasArg("raw") && server.arg("raw") == "1") {
    server.setContentLength(bytes);
    server.send(200,"application/octet-stream","");
    server.client().write(rgb, bytes);
  } else {
    uint32_t r=0,g=0,b=0; size_t pixels=fb->width*fb->height;
    size_t sample = min<size_t>(pixels, 100000); size_t stride = max<size_t>(1,pixels/sample);
    for (size_t p=0;p<pixels;p+=stride) { b+=rgb[p*3]; g+=rgb[p*3+1]; r+=rgb[p*3+2]; }
    size_t count=(pixels+stride-1)/stride;
    String h="<html><body><h2>RGB data</h2>Resolution="+String(fb->width)+"x"+String(fb->height)+"<br>Average Red="+String(r/count)+"<br>Average Green="+String(g/count)+"<br>Average Blue="+String(b/count)+"<br>Luminance="+String((r/count)*0.3+(g/count)*0.59+(b/count)*0.11)+"<br><a href='/?'>Return</a></body></html>";
    server.send(200,"text/html",h);
  }
  heap_caps_free(rgb);
  esp_camera_fb_return(fb);
}

// Similar diagnostic endpoint that samples the captured image as
// grayscale/luminance values.
void readGrayscaleImage() {
  if (!psramFound()) return server.send(503,"text/plain","error: no PSRAM available");

  // The original demo deliberately switches the OV2640 into grayscale mode,
  // renders a small ASCII representation, then restores normal JPEG mode.
  esp_camera_deinit();
  delay(200);

  camera_config_t grayConfig;
  grayConfig.ledc_channel = LEDC_CHANNEL_0;
  grayConfig.ledc_timer = LEDC_TIMER_0;
  grayConfig.pin_d0 = Y2_GPIO_NUM; grayConfig.pin_d1 = Y3_GPIO_NUM;
  grayConfig.pin_d2 = Y4_GPIO_NUM; grayConfig.pin_d3 = Y5_GPIO_NUM;
  grayConfig.pin_d4 = Y6_GPIO_NUM; grayConfig.pin_d5 = Y7_GPIO_NUM;
  grayConfig.pin_d6 = Y8_GPIO_NUM; grayConfig.pin_d7 = Y9_GPIO_NUM;
  grayConfig.pin_xclk = XCLK_GPIO_NUM; grayConfig.pin_pclk = PCLK_GPIO_NUM;
  grayConfig.pin_vsync = VSYNC_GPIO_NUM; grayConfig.pin_href = HREF_GPIO_NUM;
  grayConfig.pin_sccb_sda = SIOD_GPIO_NUM; grayConfig.pin_sccb_scl = SIOC_GPIO_NUM;
  grayConfig.pin_pwdn = PWDN_GPIO_NUM; grayConfig.pin_reset = RESET_GPIO_NUM;
  grayConfig.xclk_freq_hz = 20000000;
  grayConfig.pixel_format = PIXFORMAT_GRAYSCALE;
  grayConfig.frame_size = (framesize_t)min((int)settings.framesize, (int)FRAMESIZE_VGA);
  grayConfig.jpeg_quality = 63;
  grayConfig.fb_count = 1;
  grayConfig.fb_location = CAMERA_FB_IN_PSRAM;
  grayConfig.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  if (esp_camera_init(&grayConfig) != ESP_OK) {
    initCamera();
    return server.send(500,"text/plain","grayscale camera init failed");
  }

  sensor_t *gs = esp_camera_sensor_get();
  if (gs) {
    gs->set_brightness(gs, settings.brightness);
    gs->set_contrast(gs, settings.contrast);
    gs->set_hmirror(gs, settings.hmirror ? 1 : 0);
    gs->set_vflip(gs, settings.vflip ? 1 : 0);
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    esp_camera_deinit(); delay(200); initCamera();
    return server.send(500,"text/plain","grayscale capture failed");
  }

  const int newWidth = 115;
  const int newHeight = 42;
  const size_t newBufSize = (size_t)newWidth * newHeight;
  uint8_t *small = (uint8_t*)malloc(newBufSize);
  if (!small) {
    esp_camera_fb_return(fb);
    esp_camera_deinit(); delay(200); initCamera();
    return server.send(500,"text/plain","failed to allocate grayscale display buffer");
  }

  // Nearest-neighbour resize, matching the intent of the original demo.
  for (int y=0; y<newHeight; ++y) {
    int sy = (y * fb->height) / newHeight;
    for (int x=0; x<newWidth; ++x) {
      int sx = (x * fb->width) / newWidth;
      small[y*newWidth+x] = fb->buf[sy*fb->width+sx];
    }
  }

  uint8_t minV=255, maxV=0;
  for (size_t i=0;i<newBufSize;++i) {
    minV = min(minV, small[i]);
    maxV = max(maxV, small[i]);
  }
  const char ascii[] = "@#S%?*+;:,.  ";
  const int asciiCount = (int)(sizeof(ascii)-1);
  if (maxV == minV) maxV = minV + 1;

  String html = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>Grayscale</title>";
  html += "<style>body{font-family:monospace;margin:16px}pre{font-size:7px;line-height:.9;letter-spacing:0;margin-top:12px;overflow:auto}a{font-family:Arial,sans-serif}</style></head><body>";
  html += "<h2>Grayscale image data</h2>";
  html += "Camera frame: " + String(fb->width) + "x" + String(fb->height) + "<br>";
  html += "Display: " + String(newWidth) + "x" + String(newHeight) + " characters<br>";
  html += "Minimum: " + String(minV) + " &nbsp; Maximum: " + String(maxV);
  html += "<pre>";
  for (int y=0; y<newHeight; ++y) {
    for (int x=0; x<newWidth; ++x) {
      int idx = ((int)small[y*newWidth+x] - minV) * (asciiCount-1) / ((int)maxV-minV);
      idx = constrain(idx, 0, asciiCount-1);
      html += ascii[idx];
    }
    html += '\n';
  }
  html += "</pre><p><a href='/'>Return to dashboard</a></p></body></html>";

  server.send(200,"text/html",html);
  free(small);
  esp_camera_fb_return(fb);
  esp_camera_deinit();
  delay(200);
  initCamera();
}

// Set the remembered illumination/flash level exposed by the web UI.
void handleIllumination() {
  if (!server.hasArg("level")) return server.send(400,"text/plain","Missing level");
  illuminationBrightness = clampInt(server.arg("level").toInt(), 0, 255);
  analogWrite(FLASH_GPIO_NUM, illuminationBrightness);
  sendJson(String("{\"level\":") + illuminationBrightness + "}");
}

// ============================ MOTION ============================
// Samples a small number of pixels from the current JPEG frame's raw bytes.
// This deliberately uses JPEG byte differences rather than full image decoding:
// cheap, fast and suitable for an ESP32-CAM without an image-processing stack.
// Compare the current camera frame against a small stored baseline.
//
// This is deliberately lightweight: rather than doing expensive full-frame
// image analysis, it samples luminance at a number of locations and counts
// how many samples changed by at least motionThreshold.
//
// The baseline is updated over time, so the detector is aimed at noticeable
// scene changes rather than being a sophisticated computer-vision system.
bool motionChanged(camera_fb_t *fb) {
  if (!fb || fb->len < 256) return false;
  static uint8_t baseline[32];
  static bool baselineValid = false;
  uint16_t changed = 0;
  const size_t n = sizeof(baseline);

  for (size_t i = 0; i < n; ++i) {
    size_t pos = 32 + ((fb->len - 64 - 32) * i) / (n - 1);
    uint8_t v = fb->buf[pos];
    if (baselineValid) {
      int d = abs((int)v - (int)baseline[i]);
      if (d >= settings.motionThreshold) changed++;
    }
    baseline[i] = v;
  }
  // The first sample establishes a baseline.  There cannot be a meaningful
  // "change" result until at least one previous frame has been seen.
  if (!baselineValid) {
    baselineValid = true;
    return false;
  }
  return changed >= settings.motionMinChanged;
}

// Non-blocking motion-detection service.
//
// Called from loop() on every pass, but it actually performs a camera sample
// only every MOTION_SAMPLE_MS milliseconds.  That keeps the main loop free
// to service HTTP requests and the live stream between samples.
void motionTask() {
  // millis() intentionally wraps after a long time.  Unsigned subtraction
  // plus the signed comparison pattern used here keeps timing tests robust.
  uint32_t now = millis();
  if (motionDetected && (int32_t)(now - motionDisplayUntil) >= 0) motionDetected = false;
  // Motion detection is opt-in.  Exit immediately when it is disabled so
  // the camera is not periodically captured just for motion monitoring.
  if (!settings.motion) return;
  // Do not sample on every loop() iteration; that would consume camera and
  // CPU bandwidth and could interfere with the live stream.
  if (now - lastMotionSample < MOTION_SAMPLE_MS) return;
  lastMotionSample = now;

  // Motion sampling uses the same camera capture mechanism as normal photos.
  // The frame is inspected briefly and then returned immediately so the
  // camera buffer is available again for streaming/captures.
  camera_fb_t *fb = captureFrame();
  if (!fb) return;
  bool changed = motionChanged(fb);
  esp_camera_fb_return(fb);

  // Require a cooldown between stored motion events.  Without this, one
  // person moving in front of the camera could fill the SD card with dozens
  // of nearly identical images.
  if (changed && now - lastMotionEvent >= MOTION_COOLDOWN_MS) {
    lastMotionEvent = now;
    motionDetected = true;
    motionDisplayUntil = now + 3000;
    Serial.println("*** MOTION DETECTED ***");
    String path;
    bool stored = captureToSD("MOT", &path);
    if (stored) {
      motionCount++;
      Serial.println("Motion image saved: " + path);
    } else if (!sdReady && !spiffsReady) {
      Serial.println("Motion detected but no storage available");
    }
  }
}

// Non-blocking timelapse service.
//
// Instead of delay()'ing for the requested interval, this compares millis()
// with nextTimelapse.  That means Wi-Fi, web requests and motion monitoring
// continue running while the timelapse waits for its next capture time.
void timelapseTask() {
  // Timelapse is active only when an SD card is ready and a non-zero interval
  // has been configured.  A zero interval means "off" in the dashboard.
  if (!sdReady || settings.timelapseMs == 0) return;
  uint32_t now = millis();
  if ((int32_t)(now - nextTimelapse) < 0) return;
  // Schedule the following capture before doing the actual capture.  This
  // keeps the timing tied to the requested interval rather than accumulating
  // the capture/write time into every successive delay.
  nextTimelapse = now + settings.timelapseMs;
  String path;
  if (captureToSD("TLP", &path)) timelapseCount++;
  pruneOldestPhotos();
}

// ============================ STREAM ============================
// The live preview uses MJPEG: a sequence of independent JPEG images sent
// over one long-lived HTTP response.  The browser displays them as a stream.
//
// The stream server listens on TCP port 81, separate from the main port-80
// WebServer used by the dashboard and REST-like API endpoints.

// Service the one active MJPEG client.
//
// This is written as a polling function instead of a blocking server handler.
// The main loop calls it repeatedly, so the ESP32 can continue doing other
// work between frames.
void serviceStream() {
  static WiFiClient client;
  // There is intentionally only one stream client at a time.  If the old
  // client disconnected, accept a new connection; otherwise keep using the
  // existing connection.
  if (!client || !client.connected()) {
    WiFiClient incoming = streamServer.accept();
    if (!incoming) return;
    client = incoming;
    client.setTimeout(1000);
    client.print(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
      "Cache-Control: no-cache\r\n"
      "Pragma: no-cache\r\n"
      "Connection: close\r\n\r\n");
  }

  if (temperatureRead() > MAX_TEMP_C) { client.stop(); return; }
  camera_fb_t *fb = captureFrame();
  if (!fb) return;

  // multipart/x-mixed-replace is the simple MJPEG format used here:
  // boundary -> JPEG headers -> JPEG bytes -> CRLF -> next boundary.
  client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\nX-Frame: %lu\r\n\r\n",
                (unsigned)fb->len, (unsigned long)++streamFrameCounter);
  size_t written = client.write(fb->buf, fb->len);
  client.print("\r\n");
  esp_camera_fb_return(fb);

  if (written == 0 || !client.connected()) client.stop();
}

// ============================ WEB UI ============================
// The complete dashboard is stored in flash (PROGMEM) as a C++ raw string.
//
// A raw string literal lets us embed HTML/CSS/JavaScript without escaping
// every quote.  The browser receives this at GET /.
//
// The JavaScript talks back to the ESP32 through the /api/... endpoints and
// automatically refreshes status/file listings in the background.

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!--
  ESP32-CAM dashboard
  -------------------
  This HTML is embedded in the firmware so the board does not need an
  external web server.  The UI is deliberately simple: one page contains
  the camera preview, camera controls, GPIO controls, system statistics,
  API links and the SD-card file browser.

  The JavaScript near the bottom calls the ESP32 HTTP API endpoints.
-->
<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-CAM</title><style>
:root{font-family:system-ui,-apple-system,Segoe UI,sans-serif;color:#e8edf3;background:#10141a}body{margin:0}header{padding:14px 18px;background:#171d25;position:sticky;top:0;z-index:2}h1{font-size:20px;margin:0}main{display:grid;grid-template-columns:minmax(320px,2fr) minmax(280px,1fr);gap:16px;padding:16px;max-width:1200px;margin:auto}.card{background:#171d25;border:1px solid #27313d;border-radius:12px;padding:14px}.view{width:100%;background:#000;border-radius:10px;display:block}.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}.wide{grid-column:1/-1}label{font-size:12px;color:#9eabb9}input,select,button{width:100%;box-sizing:border-box;margin-top:4px;padding:9px;border-radius:8px;border:1px solid #364454;background:#0f141a;color:#fff}button{cursor:pointer}.row{display:flex;gap:8px}.row>*{flex:1}.stat{display:flex;justify-content:space-between;padding:4px 0;border-bottom:1px solid #222b35;font-size:13px}.files{max-height:320px;overflow:auto}.file{display:flex;justify-content:space-between;gap:8px;font-size:13px;padding:6px 0;border-bottom:1px solid #222b35}.check{display:flex;align-items:center;gap:8px;min-height:38px}.check input{width:auto;margin:0;flex:0 0 auto}.gpioState{display:inline-block;min-width:54px;font-weight:600}.linkgrid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}.linkgrid a{display:block;text-decoration:none;color:#fff;background:#0f141a;border:1px solid #364454;border-radius:8px;padding:9px;text-align:center;font-size:13px}.hint{font-size:12px;color:#9eabb9;line-height:1.5}.motionOn{font-size:18px;font-weight:700}@media(max-width:800px){main{grid-template-columns:1fr}.linkgrid{grid-template-columns:1fr 1fr}}
</style></head><body><header><h1>ESP32CAM - <span id="hostname" class="hostnameBadge">Loading...</span></h1></header><main>
<!-- Main live-preview card.  The browser connects directly to port 81. -->

<section class="card"><img id="stream" class="view"><div class="row" style="margin-top:8px"><button onclick="capture()">Capture</button><button onclick="location.reload()">Reconnect stream</button></div><p id="msg"></p></section>
<section class="card"><h3>Camera</h3><div class="grid">
<div class="wide"><label>Resolution</label><select id="resolution"><option>QQVGA</option><option>QVGA</option><option>VGA</option><option>SVGA</option><option>XGA</option><option>SXGA</option><option>UXGA</option></select></div>
<div><label>JPEG quality</label><input id="quality" type="number" min="5" max="63"></div><div><label>Brightness</label><input id="brightness" type="number" min="-2" max="2"></div>
<div><label>Contrast</label><input id="contrast" type="number" min="-2" max="2"></div><div><label>Saturation</label><input id="saturation" type="number" min="-2" max="2"></div>
<div><label>Exposure (0=auto)</label><input id="exposure" type="number" min="0" max="1200"></div><div><label>Gain (0=auto)</label><input id="gain" type="number" min="0" max="31"></div>
<div><label>Illumination 0-255</label><input id="illumination" type="number" min="0" max="255" value="0"></div>
<div><label>Motion threshold</label><input id="motionThreshold" type="number" min="1" max="60"></div><div><label>Changed samples</label><input id="motionMinChanged" type="number" min="1" max="64"></div>
<div class="wide"><label>Timelapse (seconds, 0=off)</label><input id="timelapseSec" type="number" min="0" step="1"></div>
<div class="check"><input id="hmirror" type="checkbox"><span>H mirror</span></div><div class="check"><input id="vflip" type="checkbox"><span>V flip</span></div>
<div class="check"><input id="flash" type="checkbox"><span>Flash</span></div><div class="check"><input id="motion" type="checkbox"><span>Motion capture</span></div>
<div class="wide"><button onclick="saveSettings()">Apply & save</button></div></div></section>
<section class="card"><h3>GPIO</h3><div class="stat"><span>GPIO12 input</span><strong id="gpioIn" class="gpioState">—</strong></div><div class="stat"><span>GPIO13 output</span><strong id="gpioOut" class="gpioState">—</strong></div><div class="row" style="margin-top:8px"><button onclick="setOutput(1)">Output ON</button><button onclick="setOutput(0)">Output OFF</button></div><p class="hint">GPIO12 is configured as an input with pull-up. GPIO13 is the spare output. GPIO12 must not be driven high during boot.</p></section>
<section class="card"><h3>System</h3><div id="status"></div></section>
<section class="card"><h3>Pages & API</h3><div class="linkgrid">
<a href="/">/</a><a href="/jpg">/jpg</a><a href="/jpeg">/jpeg</a><a href="/photo">/photo</a><a href="/img">/img</a><a href="/img?img=1">/img?img=1</a><a href="/stream">/stream</a><a href="/rgb">/rgb</a><a href="/graydata">/graydata</a><a href="/data">/data</a><a href="/test">/test</a><a href="/ping">/ping</a><a href="/reboot">/reboot</a><a href="/switch?on=1">/switch?on=1</a><a href="/switch?on=0">/switch?on=0</a><a href="/api/status">/api/status</a><a href="/api/settings">/api/settings</a><a href="/api/files">/api/files</a><a href="/ota">/ota</a></div><p class="hint">/stream confirms streaming is enabled and explains the separate MJPEG service on port 81. The dashboard preview uses that service automatically. /jpg is the direct still-image endpoint used by external applications.</p><p id="motionBanner" class="hint">Motion monitoring status</p></section>
<section class="card"><h3>SD photos</h3><div class="row"><button onclick="loadFiles()">Refresh</button><button onclick="clearImages()">Clear ALL stored images</button></div><div id="files" class="files"></div></section>
</main><script>
const $=id=>document.getElementById(id);
// The live MJPEG stream intentionally comes from the separate port-81
// server.  location.hostname makes this work regardless of the ESP32's
// current DHCP-assigned IP address.
$('stream').src='http://'+location.hostname+':81/stream';
// Shared fetch() helper: make an HTTP request, reject non-2xx replies and
// automatically parse the JSON returned by the ESP32 API.
async function j(url,opt){let r=await fetch(url,opt);if(!r.ok)throw new Error(await r.text());return await r.json()}
// Load the saved camera settings from the ESP32 and put them into the form.
async function load(){try{let s=await j('/api/settings');$('resolution').value=s.resolution;$('quality').value=s.quality;$('brightness').value=s.brightness;$('contrast').value=s.contrast;$('saturation').value=s.saturation;$('exposure').value=s.exposure;$('gain').value=s.gain;$('motionThreshold').value=s.motionThreshold;$('motionMinChanged').value=s.motionMinChanged;$('timelapseSec').value=s.timelapseMs/1000;$('hmirror').checked=s.hmirror;$('vflip').checked=s.vflip;$('flash').checked=s.flash;$('motion').checked=s.motion}catch(e){$('msg').textContent='Settings load failed: '+e}}
// Gather the current form values, POST them to the ESP32, then refresh
// the display so the page reflects what the firmware accepted.
async function saveSettings(){try{let q=new URLSearchParams({resolution:$('resolution').value,quality:$('quality').value,brightness:$('brightness').value,contrast:$('contrast').value,saturation:$('saturation').value,exposure:$('exposure').value,gain:$('gain').value,motionThreshold:$('motionThreshold').value,motionMinChanged:$('motionMinChanged').value,timelapseMs:String(Number($('timelapseSec').value||0)*1000),hmirror:$('hmirror').checked,vflip:$('vflip').checked,flash:$('flash').checked,motion:$('motion').checked});let s=await j('/api/settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:q.toString()});$('msg').textContent='Applied & saved: '+s.resolution;if($('illumination').value!=='')fetch('/illumination?level='+encodeURIComponent($('illumination').value));await load();await status()}catch(e){$('msg').textContent='Save failed: '+e}}
// Turn the spare GPIO13 output on or off through the HTTP API.
async function setOutput(v){try{await fetch('/switch?on='+v);await status()}catch(e){$('msg').textContent='GPIO command failed: '+e}}
// Browser capture action: ask the ESP32 to save the next image, then let
// the browser navigate to that response so the image can be viewed/downloaded.
async function capture(){location.href='/capture?save=1'}
// Poll status information every two seconds.  This keeps the dashboard
// useful as a live diagnostic panel instead of a one-time snapshot.
async function status(){try{let s=await j('/api/status');$('status').innerHTML=[['Hostname',s.hostname],['IP',s.ip],['RSSI',s.rssi+' dBm'],['Uptime',s.uptime+' s'],['Free heap',s.freeHeap],['Free PSRAM',s.freePSRAM],['SD free',Math.round(s.sdFree/1048576)+' MB'],['Last JPEG',s.lastJpegBytes+' bytes'],['Capture',s.lastCaptureMs+' ms'],['Photos',s.captures],['Motion events',s.motionEvents],['Timelapse',s.timelapse]].map(x=>'<div class="stat"><span>'+x[0]+'</span><strong>'+x[1]+'</strong></div>').join('');$('hostname').textContent=s.hostname;$('gpioIn').textContent=s.gpioInputState?'ON/HIGH':'OFF/LOW';$('gpioOut').textContent=s.gpioOutputState?'ON':'OFF';let m=document.getElementById('motionBanner');if(m){if(s.motionDetected){m.textContent='⚠ MOTION DETECTED';m.className='motionOn'}else{m.textContent=s.motionEnabled?'Motion monitoring active':'Motion capture is OFF';m.className='hint'}}}catch(e){$('status').textContent='Status unavailable'}}
// Refresh the SD-card file list shown in the dashboard.
async function loadFiles(){try{let a=await j('/api/files');$('files').innerHTML=a.reverse().map(f=>'<div class="file"><span>'+f.name.replace('/','')+' ('+Math.round(f.size/1024)+' KB)</span><span><a href="/download?name='+encodeURIComponent(f.name)+'">view</a> <button style="width:auto" onclick="del(\''+f.name+'\')">×</button></span></div>').join('')}catch(e){$('files').textContent='SD card unavailable'}}
// Delete one selected image, then refresh the list.
async function del(n){await j('/api/delete?name='+encodeURIComponent(n));loadFiles()}
// Delete all stored JPEG images after asking the user for confirmation.
async function clearImages(){if(!confirm('Delete ALL stored JPEG images?'))return;try{let r=await j('/api/clear-images');$('msg').textContent='Deleted '+r.removed+' image(s)';loadFiles()}catch(e){$('msg').textContent='Clear failed: '+e}}
// Initial page population followed by lightweight periodic polling.
// The page does not need WebSockets for this application.
load();status();loadFiles();setInterval(status,2000);setInterval(loadFiles,10000);
</script></body></html>
)HTML";

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

// ============================ WIFI ============================
// Start the ESP32 as a Wi-Fi station using the configured network.
// Power-saving is disabled because it can make a live camera stream feel
// sluggish or irregular.
void beginWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setSleep(false); // improves stream responsiveness on ESP32-CAM
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWiFiAttempt = millis();
}

// Non-blocking Wi-Fi reconnect service.
// Rather than sitting in a long while() loop whenever Wi-Fi disappears,
// the main loop retries every WIFI_RETRY_MS milliseconds.
void serviceWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  uint32_t now = millis();
  if (now - lastWiFiAttempt < WIFI_RETRY_MS) return;
  lastWiFiAttempt = now;
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// =======================================================================
// OTA (Over-The-Air) UPDATE SUPPORT
// =======================================================================
// This whole section is compiled only when ENABLE_OTA is non-zero.
// The OTA page is protected by HTTP Basic Authentication and accepts a
// compiled .bin firmware file.
#if ENABLE_OTA
bool otaStarted = false;
bool otaFailed = false;
bool otaAuthorized = false;

// Tell the browser to display an HTTP Basic Authentication prompt.
void otaRequestAuth() {
  server.requestAuthentication(BASIC_AUTH, "ESP32-CAM OTA", "Enter the OTA password");
}

// Return true only when the supplied OTA username/password are valid.
bool otaCheckAuth() {
  if (!server.authenticate(OTA_USERNAME, OTA_PASSWORD)) {
    otaRequestAuth();
    return false;
  }
  return true;
}

// Render the HTML upload page used for firmware updates.
void handleOTAPage() {
  if (!otaCheckAuth()) return;
  String html = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>ESP32-CAM V2 OTA</title>";
  html += "<style>body{font-family:Arial,sans-serif;margin:24px;max-width:720px}.box{padding:18px;border:1px solid #ccc;border-radius:10px}input,button{padding:10px;margin-top:8px} .note{color:#555}</style></head><body>";
  html += "<div class='box'><h2>ESP32-CAM V2 firmware update</h2>";
  html += "<p>Password authentication is required to access this page.</p>";
  html += "<p>Select a compiled <b>.bin</b> file and upload it.</p>";
  html += "<form method='POST' action='/ota' enctype='multipart/form-data'><input type='file' name='update' accept='.bin' required><br><button type='submit'>Upload firmware</button></form>";
  html += "<p class='note'>OTA requires an Arduino partition scheme with an OTA app slot. If your board is set to <b>Huge APP</b>, change it to a scheme that includes OTA (for example <b>Default 4MB with spiffs</b>) and reflash the sketch once via USB.</p>";
  html += "<p><a href='/'>Return to dashboard</a></p></div></body></html>";
  server.send(200,"text/html",html);
}

// Process an incoming firmware upload in chunks.
//
// The WebServer library calls this repeatedly with UPLOAD_FILE_START,
// UPLOAD_FILE_WRITE and UPLOAD_FILE_END events.  The firmware is written
// directly into the inactive OTA partition as chunks arrive.
void handleOTAUpload() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    otaAuthorized = server.authenticate(OTA_USERNAME, OTA_PASSWORD);
    otaStarted = false;
    otaFailed = false;
    if (!otaAuthorized) {
      otaFailed = true;
      Serial.println("OTA rejected: authentication failed");
      return;
    }
    Serial.printf("OTA start: %s\n", upload.filename.c_str());
    otaStarted = Update.begin(UPDATE_SIZE_UNKNOWN);
    if (!otaStarted) {
      otaFailed = true;
      Serial.println("OTA unavailable: no writable OTA partition or insufficient space");
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (otaAuthorized && !otaFailed && Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      otaFailed = true;
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!otaAuthorized) {
      otaFailed = true;
      return;
    }
    if (!otaFailed && !Update.end(true)) {
      otaFailed = true;
      Update.printError(Serial);
    } else if (!otaFailed) {
      Serial.printf("OTA complete: %u bytes\n", upload.totalSize);
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    otaFailed = true;
    otaAuthorized = false;
    Update.end();
    Serial.println("OTA upload aborted");
  }
}

// Finish the OTA HTTP request.  When the upload was successful, reboot so
// the ESP32 bootloader can start the newly written application partition.
void handleOTAFinal() {
  if (!server.authenticate(OTA_USERNAME, OTA_PASSWORD)) {
    otaAuthorized = false;
    otaFailed = true;
    otaRequestAuth();
    return;
  }
  if (!otaAuthorized || !otaStarted || otaFailed || Update.hasError()) {
    server.send(500, "text/plain", "OTA failed. Check the password, firmware file, and partition scheme.");
    return;
  }
  server.send(200, "text/plain", "OTA complete. Rebooting...");
  delay(700);
  ESP.restart();
}
#endif

// Register every HTTP route and start both servers.
// Keeping all route registration in one place makes it easy to see the
// sketch's public web/API interface at a glance.
void startServers() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/settings", HTTP_GET, [](){ if(server.args()>0){ applyArgsToSettings(); saveSettings(); } sendJson(settingsJson()); });
  server.on("/api/settings", HTTP_POST, handleSettingsSetPost);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/jpg", HTTP_GET, handleJPG);
  server.on("/jpeg", HTTP_GET, handleJpeg);
  server.on("/photo", HTTP_GET, handlePhoto);
  server.on("/img", HTTP_GET, handleImg);
  server.on("/data", HTTP_GET, handleData);
  server.on("/switch", HTTP_GET, handleSwitch);
  server.on("/illumination", HTTP_GET, handleIllumination);
  server.on("/ping", HTTP_GET, handlePing);
  server.on("/reboot", HTTP_GET, handleReboot);
  server.on("/test", HTTP_GET, handleTest);
  server.on("/rgb", HTTP_GET, readRGBImage);
  server.on("/graydata", HTTP_GET, readGrayscaleImage);
  server.on("/api/files", HTTP_GET, handleFiles);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/api/delete", HTTP_GET, handleDelete);
  server.on("/api/clear-images", HTTP_GET, handleClearImages);
  server.on("/stream", HTTP_GET, [](){
    String ip = WiFi.localIP().toString();
    String html = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>ESP32-CAM Stream</title>"
                  "<style>body{font-family:Arial,sans-serif;margin:24px;max-width:760px;line-height:1.55}.box{border:1px solid #ccc;border-radius:10px;padding:20px}code{background:#f2f2f2;padding:2px 5px;border-radius:4px}a{color:#06c}</style>"
                  "</head><body><div class='box'><h2>ESP32-CAM streaming</h2>"
                  "<p><b>Streaming has been enabled.</b></p>"
                  "<p>The ESP32-CAM runs a separate MJPEG stream server on TCP port <b>81</b>. The main HTTP server on port 80 remains available for the dashboard and other functions.</p>"
                  "<p>Compatible stream URL:</p><p><code>http://" + ip + ":81/stream</code></p>"
                  "<p>This is an MJPEG stream rather than a normal web page, so enter the address above in a compatible viewer or use the dashboard's live preview.</p>"
                  "<p><a href='http://" + ip + ":81/stream'>Open the MJPEG stream</a></p>"
                  "<p><a href='/'>Return to dashboard</a></p></div></body></html>";
    server.send(200, "text/html", html);
  });
#if ENABLE_OTA
  server.on("/ota", HTTP_GET, handleOTAPage);
  server.on("/ota", HTTP_POST, handleOTAFinal, handleOTAUpload);
#endif
  server.begin();
  streamServer.begin();
}

// Print a concise diagnostic summary after startup.
// This is especially useful when the ESP32 is headless and the Serial
// Monitor is the easiest way to discover its current IP and hardware state.
void printBootInfo() {
  Serial.println();
  Serial.println("=== ESP32-CAM V2 ===");
  Serial.printf("PSRAM: %s\n", psramFound() ? "yes" : "no");
  Serial.printf("Free heap: %u\n", ESP.getFreeHeap());
  Serial.printf("SD: %s\n", sdReady ? "ready" : "not available");
  Serial.printf("Dashboard: http://%s/\n", WiFi.localIP().toString().c_str());
  Serial.printf("Stream:    http://%s:81/stream\n", WiFi.localIP().toString().c_str());
  Serial.println();
}

// =======================================================================
// Arduino startup sequence
// =======================================================================
// setup() runs exactly once after reset/power-up.
//
// The order is intentional:
//   1. start Serial
//   2. configure GPIO defaults
//   3. restore saved settings
//   4. initialise the camera
//   5. mount filesystems
//   6. start Wi-Fi/NTP
//   7. register/start HTTP services
//   8. wait briefly for Wi-Fi and print diagnostics
// =======================================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  // Put all locally controlled pins into a known safe state before the
  // camera/network stack starts doing work.
  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW);
  pinMode(INPUT_GPIO_PIN, INPUT_PULLUP);
  pinMode(OUTPUT_GPIO_PIN, OUTPUT);
  digitalWrite(OUTPUT_GPIO_PIN, !OUTPUT_ON_LEVEL);
  pinMode(INDICATOR_LED_PIN, OUTPUT);
  digitalWrite(INDICATOR_LED_PIN, HIGH);

  // Restore persistent camera settings before initialising the sensor.
  // initCamera() then applies those values to the OV2640.
  loadSettings();

  // Camera failure is treated as fatal because most of this project depends
  // on being able to obtain frames.  A delayed restart gives the Serial
  // Monitor a chance to show the error before rebooting.
  if (!initCamera()) {
    Serial.println("Fatal: camera failed. Restarting in 5 seconds.");
    delay(5000);
    ESP.restart();
  }

  // SPIFFS is the ESP32's internal flash filesystem.  The "true" argument
  // allows it to be formatted automatically if mounting fails.
  spiffsReady = SPIFFS.begin(true);

  // SD card is the preferred bulk storage for captured photographs.
  sdReady = initSD();
  if (sdReady) pruneOldestPhotos();

  // Start Wi-Fi and ask the ESP32 time service to synchronise its clock.
  // Correct time makes saved photo filenames and timestamps much more useful.
  beginWiFi();
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.nist.gov", "time.google.com");

  // Start the normal web/API server and the separate port-81 MJPEG stream.
  startServers();

  // If timelapse is enabled, schedule the first capture one full interval
  // from now.  If disabled, the task simply remains inactive.
  nextTimelapse = millis() + (settings.timelapseMs ? settings.timelapseMs : 0);

  uint32_t wifiWaitStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiWaitStart < 15000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  printBootInfo();
}

// =======================================================================
// Main Arduino loop
// =======================================================================
// Keep loop() short and non-blocking.
//
// Each task is written so it normally does a little work and returns.  The
// timing checks inside motionTask(), timelapseTask() and serviceWiFi() mean
// they can safely be called on every pass without wasting time waiting.
//
// The only intentional tiny delay is delay(1), which yields enough CPU time
// for background ESP32/Wi-Fi housekeeping while keeping the loop responsive.
// =======================================================================
void loop() {
  // Handle ordinary HTTP requests on port 80.
  server.handleClient();

  // Handle the long-lived MJPEG client on port 81.
  serviceStream();

  // Retry Wi-Fi if it has gone away.
  serviceWiFi();

  // Check whether it is time for the next motion sample.
  motionTask();

  // Check whether it is time for the next timelapse photo.
  timelapseTask();
  // Safety timeout for the spare output: if something turned GPIO13 on,
  // automatically turn it back off after OUTPUT_TIME_LIMIT_MS.
  //
  // This prevents a forgotten web/API command from leaving an attached
  // device powered indefinitely.
  if (digitalRead(OUTPUT_GPIO_PIN) == OUTPUT_ON_LEVEL && OUTPUT_TIME_LIMIT_MS > 0 && millis() - outputChangedAt > OUTPUT_TIME_LIMIT_MS) {
    digitalWrite(OUTPUT_GPIO_PIN, !OUTPUT_ON_LEVEL);
  }
  delay(1);
}
