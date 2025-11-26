/*******************************************************************************************************************
*
*                         ESP32Cam development board demo sketch using Arduino IDE or PlatformIO
*                                   Github: https://github.com/alanesq/ESP32Cam-demo
*
*                         Tested with ESP32 board manager version 3.1.1, Arduino IDE 2.3.4
*
*     Starting point sketch for projects using the esp32cam development board with the following features
*        web server with live video streaming and RGB data from camera demonstrated.
*        sd card support using 1-bit mode (data pins are usually 2,4,12&13 but using 1bit mode only uses pin 2)
*        flash led is still available for use (pin 4) and does not flash when accessing sd card
*        Stores image in Spiffs if no sd card present
*        PWM control of the illumination/flash LED
* 
*     If ota.h file is in the sketch folder you can enable OTA updating of this sketch by setting '#define ENABLE_OTA 1'
*        in settings section.  You can then update the sketch with a BIN file via OTA by accessing page   http://x.x.x.x/ota
*        This can make updating the sketch more convenient, especially if you have installed the camera in a case etc.
*
*     GPIO:
*        You can use io pins 13 and 12 for input or output (but 12 must not be high at boot)
*        You could also use pins 1 & 3 if you do not use Serial (disable serialDebug in the settings below)
*        Pins 14, 2 & 15 should be ok to use if you are not using an SD Card
*        More info:   https://randomnerdtutorials.com/esp32-cam-ai-thinker-pinout/
*
*     You can use a MCP23017 io expander chip to give 16 gpio lines by enabling 'useMCP23017' in the setup section and connecting
*        the i2c pins to 12 and 13 on the esp32cam module.  Note: this requires the adafruit MCP23017 library to be installed.
*
*     Created using the Arduino IDE with ESP32 module installed, no additional libraries required
*        ESP32 support for Arduino IDE: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
*
*     Info on the esp32cam board:  https://randomnerdtutorials.com/esp32-cam-video-streaming-face-recognition-arduino-ide/
*                                  https://github.com/espressif/esp32-camera
*
*     To see a more advanced sketch along the same format as this one have a look at https://github.com/alanesq/CameraWifiMotion
*        which includes email support, FTP, OTA updates and motion detection
*
*     esp32cam-demo is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
*        implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*
*                                                                                 https://alanesq.github.io/
*
*******************************************************************************************************************/

#if !defined ESP32
 #error This sketch is only for an ESP32 Camera module
#endif

#include "esp_camera.h"         // https://github.com/espressif/esp32-camera
#include <Arduino.h>
#include <esp_task_wdt.h>       // watchdog timer   - see: https://iotassistant.io/esp32/enable-hardware-watchdog-timer-esp32-arduino-ide/


//   ---------------------------------------------------------------------------------------------------------



//                          ====================================== 
//                                   Enter your wifi settings
//                          ====================================== 


                        #define SSID_NAME "<WIFI SSID HERE>"
                        
                        #define SSID_PASWORD "<WIFI PASSWORD HERE>"
                        
                        #define ENABLE_OTA 0                         // If OTA updating of this sketch is enabled (requires ota.h file)
                        const String OTAPassword = "password";       // Password for performing OTA update (i.e. http://x.x.x.x/ota)





//   ---------------------------------------------------------------------------------------------------------

    // Required by PlatformIO

    // forward declarations
      bool initialiseCamera(bool);            // this sets up and enables the camera (if bool=1 standard settings are applied but 0 allows you to apply custom settings)
      bool cameraImageSettings(bool);         // this applies the image settings to the camera (brightness etc.)
      void changeResolution(framesize_t);     // change camera resolution
      String localTime();                     // returns the current time as a String
      void flashLED(int);                     // flashes the onboard indicator led
      byte storeImage();                      // stores an image in Spiffs or SD card
      void handleRoot();                      // the root web page
      void handlePhoto();                     // web page to capture an image from camera and save to spiffs or sd card
      bool handleImg();                       // Display a previously stored image 
      void handleNotFound();                  // if invalid web page is requested
      void readRGBImage();                    // demo capturing an image and reading its raw RGB data
      bool getNTPtime(int);                   // handle the NTP real time clock
      bool handleJPG();                       // display a raw jpg image
      void handleJpeg();                      // display a raw jpg image which periodically refreshes
      void handleStream();                    // stream live video (note: this can get the camera very hot)
      int requestWebPage(String*, String*, int);  // procedure allowing the sketch to read a web page its self
      void handleTest();                      // test procedure for experimenting with bits of code etc.
      void handleReboot();                    // handle request to restart device
      void handleData();                      // the root web page requests this periodically via Javascript in order to display updating information
      void readGrayscaleImage();              // demo capturing a grayscale image and reading its raw RGB data
      void resize_esp32cam_image_buffer(uint8_t*, int, int, uint8_t*, int, int);    // this resizes a captured grayscale image (used by above)


// ---------------------------------------------------------------
//                           -SETTINGS
// ---------------------------------------------------------------

 char* stitle = "ESPcamDemo";                           // title of this sketch
 char* sversion = "26Nov25";                            // Sketch version

 const float MAX_TEMP_C = 75.0;                         // ESP temperate above which live streaming is stopped
 
 framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_SVGA;         // default camera resolution
    //           Resolutions available:
    //               160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA), 240X240,
    //               320x240 (QVGA), 400x296 (CIF), 640x480 (VGA default), 800x600 (SVGA),
    //               1024x768 (XGA), 1280x1024 (SXGA), 1600x1200 (UXGA)

 int imageQuality = 5;                                  // Jpeg image quality 0-63 (lower = better)

 #define WDT_TIMEOUT 60                                 // timeout of watchdog timer (seconds) 

 const bool sendRGBfile = 0;                            // if set to 1 '/rgb' will just return raw rgb data which can then be saved as a file rather than display a HTML pag

 uint16_t dataRefresh = 2;                              // how often to refresh data on root web page (seconds)
 uint16_t imagerefresh = 2;                             // how often to refresh the image on root web page (seconds)

 const bool serialDebug = 1;                            // show debug info. on serial port (1=enabled, disable if using pins 1 and 3 as gpio)
 const int serialSpeed = 115200;                        // Serial data speed to use

 #define useMCP23017 0                                  // set if MCP23017 IO expander chip is being used (on pins 12 and 13)

 // Camera related
   bool flashRequired = 0;                              // If flash to be used when capturing image (1 = yes)
   int cameraImageExposure = 0;                         // Camera exposure (0 - 1200)   If gain and exposure both set to zero then auto adjust is enabled
   int cameraImageGain = 0;                             // Image gain (0 - 30)
   int cameraImageBrightness = 0;                       // Image brightness (-2 to +2)
   const int camChangeDelay = 200;                      // delay when deinit camera executed

 const int TimeBetweenStatus = 600;                     // speed of flashing system running ok status light (milliseconds)

 const int indicatorLED = 33;                           // onboard small LED pin (33)
 const bool flashIndicatorLED = 1;                      // if led is to flash to indicate camera is operating ok

 // Bright LED (Flash)
   const int brightLED = 4;                             // onboard Illumination/flash LED pin (4)
   int brightLEDbrightness = 0;                         // initial brightness (0 - 255)
   const int ledFreq = 5000;                            // PWM settings
   const int ledChannel = 15;                           // camera uses timer1
   const int ledRresolution = 8;                        // resolution (8 = from 0 to 255)

 const int iopinA = 13;                                 // general io pin 13
 const int iopinB = 12;                                 // general io pin 12 (must not be high at boot)


// camera settings (for the standard - OV2640 - CAMERA_MODEL_AI_THINKER)
// see: https://randomnerdtutorials.com/esp32-cam-camera-pin-gpios/
// set camera resolution etc. in 'initialiseCamera()' and 'cameraImageSettings()'
 #define CAMERA_MODEL_AI_THINKER
 #define PWDN_GPIO_NUM     32      // power to camera (on/off)
 #define RESET_GPIO_NUM    -1      // -1 = not used
 #define XCLK_GPIO_NUM      0
 #define SIOD_GPIO_NUM     26      // i2c sda
 #define SIOC_GPIO_NUM     27      // i2c scl
 #define Y9_GPIO_NUM       35
 #define Y8_GPIO_NUM       34
 #define Y7_GPIO_NUM       39
 #define Y6_GPIO_NUM       36
 #define Y5_GPIO_NUM       21
 #define Y4_GPIO_NUM       19
 #define Y3_GPIO_NUM       18
 #define Y2_GPIO_NUM        5
 #define VSYNC_GPIO_NUM    25      // vsync_pin
 #define HREF_GPIO_NUM     23      // href_pin
 #define PCLK_GPIO_NUM     22      // pixel_clock_pin
 camera_config_t config;           // camera settings


// ******************************************************************************************************************


//#include "esp_camera.h"         // https://github.com/espressif/esp32-camera
// #include "camera_pins.h"
#include <WString.h>            // this is required for base64.h otherwise get errors with esp32 core 1.0.6 - jan23
#include <base64.h>             // for encoding buffer to display image on page
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include "driver/ledc.h"        // used to configure pwm on illumination led

// NTP - Internet time - see - https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/
  #include "time.h"
  struct tm timeinfo;
  const char* ntpServer = "pool.ntp.org";
  const char* TZ_INFO    = "GMT+0BST-1,M3.5.0/01:00:00,M10.5.0/02:00:00";  // enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
  long unsigned lastNTPtime;
  time_t now;
   
// spiffs used to store images if no sd card present
 #include <SPIFFS.h>
 #include <FS.h>                               // gives file access on spiffs

WebServer server(80);                          // serve web pages on port 80

// Used to disable brownout detection
 #include "soc/soc.h"
 #include "soc/rtc_cntl_reg.h"

// sd-card
 #include "SD_MMC.h"                           // sd card - see https://randomnerdtutorials.com/esp32-cam-take-photo-save-microsd-card/
 #include <SPI.h>
 #include <FS.h>                               // gives file access
 #define SD_CS 5                               // sd chip select pin = 5

// MCP23017 IO expander on pins 12 and 13 (optional)
 #if useMCP23017 == 1
   #include <Wire.h>
   #include "Adafruit_MCP23017.h"
   Adafruit_MCP23017 mcp;
   // Wire.setClock(1700000);                  // set frequency to 1.7mhz
 #endif

// Define some global variables:
 uint32_t lastStatus = millis();               // last time status light changed status (to flash all ok led)
 bool sdcardPresent;                           // flag if an sd card is detected
 int imageCounter;                             // image file name on sd card counter
 const String spiffsFilename = "/image.jpg";   // image name to use when storing in spiffs
 String ImageResDetails = "Unknown";           // image resolution info
 int currentRes = 0;                           // current selected camera resolution

// OTA Stuff
  bool OTAEnabled = 0;                         // flag to show if OTA has been enabled (via supply of password in http://x.x.x.x/ota)
  #if ENABLE_OTA
      void sendHeader(WiFiClient &client, char* hTitle);      // forward declarations
      void sendFooter(WiFiClient &client);
      #include "ota.h"                         // Over The Air updates (OTA)
  #endif

// ---------------------------------------------------------------
//    -SETUP     SETUP     SETUP     SETUP     SETUP     SETUP
// ---------------------------------------------------------------

void setup() {

 if (serialDebug) {
   Serial.begin(serialSpeed);                     // Start serial communication
   // Serial.setDebugOutput(true);

   Serial.println("\n\n\n");                      // line feeds
   Serial.println("-----------------------------------");
   Serial.printf("Starting - %s - %s \n", stitle, sversion);
   Serial.println("-----------------------------------");
   // Serial.print("Reset reason: " + ESP.getResetReason());
 }

 WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);     // Turn-off the 'brownout detector'

 // small indicator led on rear of esp32cam board
   pinMode(indicatorLED, OUTPUT);
   digitalWrite(indicatorLED,HIGH);

 // Connect to wifi
   digitalWrite(indicatorLED,LOW);               // small indicator led on
   if (serialDebug) {
     Serial.print("\nConnecting to ");
     Serial.print(SSID_NAME);
     Serial.print("\n   ");
   }
   WiFi.begin(SSID_NAME, SSID_PASWORD);
   while (WiFi.status() != WL_CONNECTED) {
       delay(500);
       if (serialDebug) Serial.print(".");
   }
   if (serialDebug) {
     Serial.print("\nWiFi connected, ");
     Serial.print("IP address: ");
     Serial.println(WiFi.localIP());
   }
   server.enableCORS();   // allow html to request pages without it being blocked
   server.begin();                               // start web server
   digitalWrite(indicatorLED,HIGH);              // small indicator led off

 // define the web pages (i.e. call these procedures when url is requested)
   server.on("/", handleRoot);                   // root page
   server.on("/data", handleData);               // suplies data to periodically update root (AJAX)
   server.on("/jpg", handleJPG);                 // capture image and send as jpg
   server.on("/jpeg", handleJpeg);                // show updating image
   server.on("/stream", handleStream);           // stream live video
   server.on("/photo", handlePhoto);             // save image to sd card
   server.on("/img", handleImg);                 // show image from sd card
   server.on("/rgb", readRGBImage);              // demo converting image to RGB
   server.on("/graydata", readGrayscaleImage);   // look at grayscale image data
   server.on("/test", handleTest);               // Testing procedure
   server.on("/reboot", handleReboot);           // restart device
   server.on("/ping", handlePing);               // for checking camera is responding
   server.on("/switch", handleSwitch);           // switch gpio pin via a url
   server.onNotFound(handleNotFound);            // invalid url requested
#if ENABLE_OTA   
  server.on("/ota", handleOTA);                 // ota updates web page
#endif  

 // NTP - internet time
   if (serialDebug) Serial.println("\nGetting real time (NTP)");
   configTime(0, 0, ntpServer);
   setenv("TZ", TZ_INFO, 1);
   if (getNTPtime(10)) {  // wait up to 10 sec to sync
   } else {
     if (serialDebug) Serial.println("Time not set");
   }
   lastNTPtime = time(&now);

 // set up camera
     if (serialDebug) Serial.print(("\nInitialising camera: "));
     if (initialiseCamera(1)) {           // apply settings from 'config' and start camera
       if (serialDebug) Serial.println("OK");
     }
     else {
       if (serialDebug) Serial.println("failed");
     }

 // Spiffs - for storing images without an sd card
 //       see: https://circuits4you.com/2018/01/31/example-of-esp8266-flash-file-system-spiffs/
   if (!SPIFFS.begin(true)) {
     if (serialDebug) Serial.println(("An Error has occurred while mounting SPIFFS - restarting"));
     delay(5000);
     ESP.restart();                               // restart and try again
     delay(5000);
   } else {
     // SPIFFS.format();      // wipe spiffs
     delay(5000);
     if (serialDebug) {
       Serial.print(("SPIFFS mounted successfully: "));
       Serial.printf("total bytes: %d , used: %d \n", SPIFFS.totalBytes(), SPIFFS.usedBytes());
     }
   }

 // SD Card - if one is detected set 'sdcardPresent' High
     if (!SD_MMC.begin("/sdcard", true)) {        // if loading sd card fails
       // note: ('/sdcard", true)' = 1bit mode - see: https://dr-mntn.net/2021/02/using-the-sd-card-in-1-bit-mode-on-the-esp32-cam-from-ai-thinker
       if (serialDebug) Serial.println("No SD Card detected");
       sdcardPresent = 0;                        // flag no sd card available
     } else {
       uint8_t cardType = SD_MMC.cardType();
       if (cardType == CARD_NONE) {              // if invalid card found
           if (serialDebug) Serial.println("SD Card type detect failed");
           sdcardPresent = 0;                    // flag no sd card available
       } else {
         // valid sd card detected
         uint16_t SDfreeSpace = (uint64_t)(SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024 * 1024);
         if (serialDebug) Serial.printf("SD Card found, free space = %dmB \n", SDfreeSpace);
         sdcardPresent = 1;                      // flag sd card available
       }
     }
     fs::FS &fs = SD_MMC;                        // sd card file system

 // discover the number of image files already stored in '/img' folder of the sd card and set image file counter accordingly
   imageCounter = 0;
   if (sdcardPresent) {
     int tq=fs.mkdir("/img");                    // create the '/img' folder on sd card (in case it is not already there)
     if (!tq) {
       if (serialDebug) Serial.println("Unable to create IMG folder on sd card");
     }

     // open the image folder and step through all files in it
       File root = fs.open("/img");
       while (true)
       {
           File entry =  root.openNextFile();    // open next file in the folder
           if (!entry) break;                    // if no more files in the folder
           imageCounter ++;                      // increment image counter
           entry.close();
       }
       root.close();
       if (serialDebug) Serial.printf("Image file count = %d \n",imageCounter);
   }

 // define i/o pins
   pinMode(indicatorLED, OUTPUT);            // defined again as sd card config can reset it
   digitalWrite(indicatorLED,HIGH);          // led off = High
   pinMode(iopinA, INPUT);                   // pin 13 - free io pin, can be used for input or output
   pinMode(iopinB, OUTPUT);                  // pin 12 - free io pin, can be used for input or output (must not be high at boot)
   digitalWrite(iopinB, LOW);                // set pin 12 low asap to ensure it does not change esp boot mode

 // MCP23017 io expander (requires adafruit MCP23017 library)
 #if useMCP23017 == 1
   Wire.begin(12,13);             // use pins 12 and 13 for i2c
   mcp.begin(&Wire);              // use default address 0
   mcp.pinMode(0, OUTPUT);        // Define GPA0 (physical pin 21) as output pin
   mcp.pinMode(8, INPUT);         // Define GPB0 (physical pin 1) as input pin
   mcp.pullUp(8, HIGH);           // turn on a 100K pullup internally
   // change pin state with   mcp.digitalWrite(0, HIGH);
   // read pin state with     mcp.digitalRead(8)
 #endif

// configure PWM for the illumination LED
  pinMode(brightLED, OUTPUT);
  analogWrite(brightLED, brightLEDbrightness);

// ESP32 Watchdog timer -    Note: esp32 board manager v3.x.x requires different code
  #if defined ESP32
    esp_task_wdt_deinit();                  // ensure a watchdog is not already configured
    #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR == 3
      // v3 board manager detected
        if (serialDebug) Serial.println("Watchdog timer: v3 esp32 board manager detected");
        esp_task_wdt_config_t wdt_config = {
            .timeout_ms = WDT_TIMEOUT * 1000, // Convert seconds to milliseconds
            .idle_core_mask = 1 << 0,         // Which core to monitor
            .trigger_panic = true             // Enable panic
        };
      // Initialize the WDT with the configuration structure
        esp_task_wdt_init(&wdt_config);       // Pass the pointer to the configuration structure
        esp_task_wdt_add(NULL);               // Add current thread to WDT watch    
        esp_task_wdt_reset();                 // reset timer
        if (serialDebug) Serial.println("Watchdog Timer initialized");
    #else
      // pre v3 board manager assumed
        if (serialDebug) Serial.println("Watchdog timer: Older esp32 board manager detected");
        esp_task_wdt_init(WDT_TIMEOUT, true);                      //enable panic so ESP32 restarts
        esp_task_wdt_add(NULL);                                    //add current thread to WDT watch   
    #endif
  #endif  

 // startup complete
   if (serialDebug) Serial.println("\nStarted...");
   flashLED(2);     // flash the onboard indicator led
   analogWrite(brightLED, 64);    // change bright LED
   delay(200);
   analogWrite(brightLED, 0);    // change bright LED

}  // setup


// ----------------------------------------------------------------
//   -LOOP     LOOP     LOOP     LOOP     LOOP     LOOP     LOOP
// ----------------------------------------------------------------


void loop() {

 server.handleClient();          // handle any incoming web page requests






 //                           <<< YOUR CODE HERE >>>






//  //  Capture an image and save to sd card every 5 seconds (i.e. time lapse)
//      static uint32_t lastCamera = millis();
//      if ( ((unsigned long)(millis() - lastCamera) >= 5000) && sdcardPresent ) {
//        lastCamera = millis();     // reset timer
//        storeImage();              // save an image to sd card
//        if (serialDebug) Serial.println("Time lapse image captured");
//      }

 // flash status LED to show sketch is running ok
   if ((unsigned long)(millis() - lastStatus) >= TimeBetweenStatus) {
     lastStatus = millis();                                               // reset timer
     esp_task_wdt_reset();                                                // reset watchdog timer (to prevent system restart)
     if (flashIndicatorLED) digitalWrite(indicatorLED,!digitalRead(indicatorLED));     // flip indicator led status
   }

}  // loop


// ----------------------------------------------------------------
//                        Initialise the camera
// ----------------------------------------------------------------
// returns TRUE if successful
// reset - if set to 1 all settings are reconfigured
//         if set to zero you can change the settings and call this procedure to apply them

bool initialiseCamera(bool reset) {

// set the camera parameters in 'config'
if (reset) {
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
   // variations in version of esp32 board manager (v3 changed the names for some reason)
     #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR == 3  
        config.pin_sccb_sda = SIOD_GPIO_NUM;    // v3.x
        config.pin_sccb_scl = SIOC_GPIO_NUM;     
     #else
        config.pin_sscb_sda = SIOD_GPIO_NUM;    // pre v3
        config.pin_sscb_scl = SIOC_GPIO_NUM; 
     #endif
   config.pin_pwdn = PWDN_GPIO_NUM;
   config.pin_reset = RESET_GPIO_NUM;   
   config.pin_pwdn = PWDN_GPIO_NUM;
   config.pin_reset = RESET_GPIO_NUM;
   config.xclk_freq_hz = 10000000;               // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
   config.pixel_format = PIXFORMAT_JPEG;                         // colour jpg format
   config.frame_size = FRAME_SIZE_IMAGE;         // Image sizes: 160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA), 320x240 (QVGA),
                                                 //              400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA), 1024x768 (XGA), 1280x1024 (SXGA),
                                                 //              1600x1200 (UXGA)
   config.jpeg_quality = imageQuality;                     // 0-63 lower number means higher quality (can cause failed image capture if set too low at higher resolutions)
   config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
   //config.fb_location = CAMERA_FB_IN_PSRAM;      // store the captured frame in PSRAM
   config.fb_count = 1;                          // if more than one, i2s runs in continuous mode. Use only with JPEG
}

   // check the esp32cam board has a psram chip installed (extra memory used for storing captured images)
   //    Note: if not using "AI thinker esp32 cam" in the Arduino IDE, PSRAM must be enabled
    if (!psramFound()) {
      if (serialDebug) Serial.println("Warning: No PSRam found so defaulting to smaller image size 'VGA' in DRAM");
      config.frame_size = FRAMESIZE_VGA;     
      config.fb_location = CAMERA_FB_IN_DRAM;
    } else {
      // If PSRAM IS found, explicitly set fb_location  
      config.fb_location = CAMERA_FB_IN_PSRAM;
    }

   esp_err_t camerr = esp_camera_init(&config);  // initialise the camera
   if (camerr != ESP_OK) {
     if (serialDebug) Serial.printf("ERROR: Camera init failed with error 0x%x", camerr);
   }

   cameraImageSettings(0);                       // apply the camera image settings

   return (camerr == ESP_OK);                    // return boolean result of camera initialisation
}


// ----------------------------------------------------------------
//                   -Change camera image settings
// ----------------------------------------------------------------
// Adjust image properties (brightness etc.)
// Defaults to auto adjustments if exposure and gain are both set to zero
// if 'flush' is set then several frames are captured to ensure settings take effect straight away
// - Returns TRUE if successful
// More info: https://randomnerdtutorials.com/esp32-cam-ov2640-camera-settings/
//            interesting info on exposure times here: https://github.com/raduprv/esp32-cam_ov2640-timelapse

bool cameraImageSettings(bool flush) {   

  if (serialDebug) Serial.println("Applying camera settings");

   sensor_t *s = esp_camera_sensor_get();
   // something to try?:     if (s->id.PID == OV3660_PID)
   if (s == NULL) {
     if (serialDebug) Serial.println("Error: problem reading camera sensor settings");
     return 0;
   }

   // if both set to zero enable auto adjust
   if (cameraImageExposure == 0 && cameraImageGain == 0) {
     // enable auto adjust
       s->set_gain_ctrl(s, 1);                       // auto gain on
       s->set_exposure_ctrl(s, 1);                   // auto exposure on 
       s->set_saturation(s, -1);                     // Slightly decrease saturation
       s->set_contrast(s, -1);                       // Slightly decrease contrast
       s->set_whitebal(s, 1);                        // Enable white balance
       s->set_awb_gain(s, 1);                        // Auto White Balance enable (0 or 1)
       s->set_brightness(s, cameraImageBrightness);  // (-2 to 2) - set brightness
   } else {
     // Apply manual settings
       s->set_gain_ctrl(s, 0);                       // auto gain off
       s->set_whitebal(s, 1);                        // Enable white balance
       s->set_awb_gain(s, 1);                        // Auto White Balance enable (0 or 1)
       s->set_saturation(s, -1);                     // Slightly decrease saturation
       s->set_contrast(s, -1);                       // Slightly decrease contrast
       s->set_whitebal(s, 1);                        // Enable auto white balanc
       s->set_exposure_ctrl(s, 0);                   // auto exposure off
       s->set_brightness(s, cameraImageBrightness);  // (-2 to 2) - set brightness
       s->set_agc_gain(s, cameraImageGain);          // set gain manually (0 - 30)
       s->set_aec_value(s, cameraImageExposure);     // set exposure manually  (0-1200)
   }
   
   //s->set_vflip(s, 1);                               // flip image vertically
   //s->set_hmirror(s, 1);                             // flip image horizontally

  // Discard initial frames to ensure new settings apply 
  // This is a pain as it takes a while but without this the first few frames will not have the new setting applied
  if (flush == 1) {
    for (int i = 0; i < 5; i++) {
        camera_fb_t *fb = esp_camera_fb_get();
        esp_camera_fb_return(fb);
    }
  }

   return 1;
}  // cameraImageSettings


//    // More camera settings available:
//    // If you enable gain_ctrl or exposure_ctrl it will prevent a lot of the other settings having any effect
//    // more info on settings here: https://randomnerdtutorials.com/esp32-cam-ov2640-camera-settings/
//    s->set_gain_ctrl(s, 0);                       // auto gain off (1 or 0)
//    s->set_exposure_ctrl(s, 0);                   // auto exposure off (1 or 0)
//    s->set_agc_gain(s, 1);                        // set gain manually (0 - 30)
//    s->set_aec_value(s, 1);                       // set exposure manually  (0-1200)
//    s->set_vflip(s, 1);                           // Invert image (0 or 1)
//    s->set_quality(s, 10);                        // (0 - 63)
//    s->set_gainceiling(s, GAINCEILING_32X);       // Image gain (GAINCEILING_x2, x4, x8, x16, x32, x64 or x128)
//    s->set_brightness(s, 0);                      // (-2 to 2) - set brightness
//    s->set_lenc(s, 1);                            // lens correction? (1 or 0)
//    s->set_saturation(s, 0);                      // (-2 to 2)
//    s->set_contrast(s, 0);                        // (-2 to 2)
//    s->set_sharpness(s, 0);                       // (-2 to 2)
//    s->set_hmirror(s, 0);                         // (0 or 1) flip horizontally
//    s->set_colorbar(s, 0);                        // (0 or 1) - show a testcard
//    s->set_special_effect(s, 0);                  // (0 to 6?) apply special effect
//    s->set_whitebal(s, 0);                        // white balance enable (0 or 1)
//    s->set_awb_gain(s, 1);                        // Auto White Balance enable (0 or 1)
//    s->set_wb_mode(s, 0);                         // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
//    s->set_dcw(s, 0);                             // downsize enable? (1 or 0)?
//    s->set_raw_gma(s, 1);                         // (1 or 0)
//    s->set_aec2(s, 0);                            // automatic exposure sensor?  (0 or 1)
//    s->set_ae_level(s, 0);                        // auto exposure levels (-2 to 2)
//    s->set_bpc(s, 0);                             // black pixel correction
//    s->set_wpc(s, 0);                             // white pixel correction


// ----------------------------------------------------------------
//             returns the current time as a String
// ----------------------------------------------------------------
//   see: https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
String localTime() {
 struct tm timeinfo;
 char ttime[40];
 if(!getLocalTime(&timeinfo)) return"Failed to obtain time";
 strftime(ttime,40,  "%A %B %d %Y %H:%M:%S", &timeinfo);
 return ttime;
}


// ----------------------------------------------------------------
//        flash the indicator led 'reps' number of times
// ----------------------------------------------------------------
void flashLED(int reps) {
 for(int x=0; x < reps; x++) {
   digitalWrite(indicatorLED,LOW);
   delay(1000);
   digitalWrite(indicatorLED,HIGH);
   delay(500);
 }
}


// ----------------------------------------------------------------
//      send standard html header (i.e. start of web page)
// ----------------------------------------------------------------
void sendHeader(WiFiClient &client, char* hTitle) {
    // Start page
      client.write("HTTP/1.1 200 OK\r\n");
      client.write("Content-Type: text/html\r\n");
      client.write("Connection: close\r\n");
      client.write("\r\n");
      client.write("<!DOCTYPE HTML><html lang='en'>\n");
    // HTML / CSS
      client.printf(R"=====(
        <head>
          <meta name='viewport' content='width=device-width, initial-scale=1.0'>
          <title>%s</title>
          <style>
            body {
              color: black;
              background-color: #FFFF00;
              text-align: center;
            }
            input, select {
              background-color: #FF9900;
              border: 2px #FF9900;
              color: blue;
              padding: 3px 6px;
              margin: 3px;
              text-align: center;
              text-decoration: none;
              display: inline-block;
              font-size: 16px;
              cursor: pointer;
              border-radius: 7px;
            }
            input:hover {
              background-color: #FF4400;
            }
          </style>
        </head>
        <body>
        <h1 style='color:red;'>%s</H1>
      )=====", hTitle, hTitle);
}


// ----------------------------------------------------------------
//      send a standard html footer (i.e. end of web page)
// ----------------------------------------------------------------
void sendFooter(WiFiClient &client) {
  client.write("</body></html>\n");
  delay(3);
  client.stop();
}


// ----------------------------------------------------------------
//  send line of text to both serial port and web page - used by readRGBImage
// ----------------------------------------------------------------
void sendText(WiFiClient &client, String theText) {
     if (!sendRGBfile) client.print(theText + "<br>\n");
     if (serialDebug || theText.indexOf("error") > 0) Serial.println(theText);   // if text contains "error"
}


// ----------------------------------------------------------------
//                        reset the camera
// ----------------------------------------------------------------
// either hardware(1) or software(0)
void resetCamera(bool type = 0) {
  if (type == 1) {
    // power cycle the camera module (handy if camera stops responding)
      digitalWrite(PWDN_GPIO_NUM, HIGH);    // turn power off to camera module
      delay(300);
      digitalWrite(PWDN_GPIO_NUM, LOW);
      delay(300);
      initialiseCamera(1);
    } else {
    // reset via software (handy if you wish to change resolution or image type etc. - see test procedure)
      esp_camera_deinit();
      delay(camChangeDelay);
      initialiseCamera(1);
    }
}


// ----------------------------------------------------------------
//                    -change image resolution
// ----------------------------------------------------------------
// Change camera resolution
// Resolutions:  160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA),
//               320x240 (QVGA), 400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA),
//               1024x768 (XGA), 1280x1024 (SXGA), 1600x1200 (UXGA)
void changeResolution(framesize_t newRes) {
  esp_camera_deinit();     // disable camera
  delay(camChangeDelay);
  FRAME_SIZE_IMAGE = newRes;

  initialiseCamera(1);
  if (serialDebug) Serial.println("Camera resolution changed");
  ImageResDetails = "Unknown";   // set next time image captured
}


// ----------------------------------------------------------------
//     Capture image from camera and save to spiffs or sd card
// ----------------------------------------------------------------
// returns 0 if failed, 1 if stored in spiffs, 2 if stored on sd card

byte storeImage() {

 byte sRes = 0;                // result flag
 fs::FS &fs = SD_MMC;          // sd card file system

 // capture the image from camera
   int currentBrightness = brightLEDbrightness;
   if (flashRequired) {
      analogWrite(brightLED, 255);   // change LED brightness (0 - 255)
      delay(100);
   }
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();

   if (flashRequired){
      delay(100);
      analogWrite(brightLED, currentBrightness);   // change LED brightness back to previous state
   }
   if (!fb) {
     if (serialDebug) Serial.println("Error: Camera capture failed");
     return 0;
   }

 // save image to Spiffs
   if (!sdcardPresent) {
     if (serialDebug) Serial.println("Storing image to spiffs only");
     SPIFFS.remove(spiffsFilename);                         // if file name already exists delete it
     File file = SPIFFS.open(spiffsFilename, FILE_WRITE);   // create new file
     if (!file) {
       if (serialDebug) Serial.println("Failed to create file in Spiffs - will format and try again");
       if (!SPIFFS.format()) {                              // format spiffs
         if (serialDebug) Serial.println("Spiffs format failed");
         return 0;
       } else {
         file = SPIFFS.open(spiffsFilename, FILE_WRITE);    // try again to create new file
         if (!file) {
           if (serialDebug) Serial.println("Still unable to create file in spiffs");
           return 0;
         }
       }
     }
     if (file) {       // if file has been created ok write image data to it
       if (file.write(fb->buf, fb->len)) {
         sRes = 1;    // flag as saved ok
       } else {
         if (serialDebug) Serial.println("Error: failed to write image data to spiffs file");
         file.close();
         return 0;
       }
     }
     esp_camera_fb_return(fb);                               // return camera frame buffer
     if (sRes == 1 && serialDebug) {
       Serial.print("The picture has been saved to Spiffs as " + spiffsFilename);
       Serial.print(" - Size: ");
       Serial.print(file.size());
       Serial.println(" bytes");
     }
     file.close();
   }


 // save the image to sd card
   if (sdcardPresent) {
     if (serialDebug) Serial.printf("Storing image #%d to sd card \n", imageCounter);
     String SDfilename = "/img/" + String(imageCounter + 1) + ".jpg";              // build the image file name
     File file = fs.open(SDfilename, FILE_WRITE);                                  // create file on sd card
     if (!file) {
       if (serialDebug) Serial.printf("Error: Failed to create file on sd-card:  %s\n", spiffsFilename.c_str());
     } else {
       if (file.write(fb->buf, fb->len)) {                                         // File created ok so save image to it
         if (serialDebug) Serial.println("Image saved to sd card");
         imageCounter ++;                                                          // increment image counter
         sRes = 2;    // flag as saved ok
       } else {
         if (serialDebug) Serial.println("Error: failed to save image data file on sd card");
       }
       file.close();              // close image file on sd card
     }
   }

 esp_camera_fb_return(fb);        // return frame so memory can be released

 return sRes;

} // storeImage


// ----------------------------------------------------------------
//            -Action any user input on root web page
// ----------------------------------------------------------------

void rootUserInput(WiFiClient &client) {         

    // if button1 was pressed (toggle io pin A)
    //        Note:  if using an input box etc. you would read the value with the command:    String Bvalue = server.arg("demobutton1");

    // if image resultion was selected
      if (server.hasArg("submit")) {           // only if submit button was pressed
        if (serialDebug) Serial.println("SUBMIT has been clicked");
        if (server.hasArg("resolution")) {   
          String option = server.arg("resolution");
          if (serialDebug) Serial.println("camera resolution selected = " + option);
          framesize_t qres = FRAMESIZE_SVGA;   // default
          if (option == "QVGA") qres = FRAMESIZE_QVGA;
          if (option == "VGA") qres = FRAMESIZE_VGA;
          if (option == "SVGA") qres = FRAMESIZE_SVGA;
          if (option == "XGA") qres = FRAMESIZE_XGA;
          if (option == "SXGA") qres = FRAMESIZE_SXGA;
          if (qres != FRAME_SIZE_IMAGE) changeResolution(qres);    // change cameras resolution
        }
      }

    // if button1 was pressed (toggle io pin B)
      if (server.hasArg("button1")) {
        if (serialDebug) Serial.println("Button 1 pressed");
        digitalWrite(iopinB,!digitalRead(iopinB));             // toggle output pin on/off
      }

    // if button2 was pressed (Cycle illumination LED)
      if (server.hasArg("button2")) {
        if (serialDebug) Serial.println("Button 2 pressed");
        if (brightLEDbrightness == 0) brightLEDbrightness = 10;                // turn led on dim
        else if (brightLEDbrightness == 10) brightLEDbrightness = 40;          // turn led on medium
        else if (brightLEDbrightness == 40) brightLEDbrightness = 255;         // turn led on full
        else brightLEDbrightness = 0;                                          // turn led off
        analogWrite(brightLED, brightLEDbrightness);
      }

    // if button3 was pressed (toggle flash)
      if (server.hasArg("button3")) {
        if (serialDebug) Serial.println("Button 3 pressed");
        flashRequired = !flashRequired;
      }

    // if button3 was pressed (format SPIFFS)
      if (server.hasArg("button4")) {
        if (serialDebug) Serial.println("Button 4 pressed");
        if (!SPIFFS.format()) {
          if (serialDebug) Serial.println("Error: Unable to format Spiffs");
        } else {
          if (serialDebug) Serial.println("Spiffs memory has been formatted");
        }
      }

    // if brightness was adjusted - cameraImageBrightness
        if (server.hasArg("bright")) {
          String Tvalue = server.arg("bright");   // read value
          if (Tvalue != NULL) {
            int val = Tvalue.toInt();
            if (val >= -2 && val <= 2 && val != cameraImageBrightness) {
              if (serialDebug) Serial.printf("Brightness changed to %d\n", val);
              cameraImageBrightness = val;
              cameraImageSettings(0);           // Apply camera image settings
            }
          }
        }

    // if exposure was adjusted - cameraImageExposure
        if (server.hasArg("exp")) {
          if (serialDebug) Serial.println("Exposure has been changed");
          String Tvalue = server.arg("exp");   // read value
          if (Tvalue != NULL) {
            int val = Tvalue.toInt();
            if (val >= 0 && val <= 1200 && val != cameraImageExposure) {
              if (serialDebug) Serial.printf("Exposure changed to %d\n", val);
              cameraImageExposure = val;
              cameraImageSettings(0);           // Apply camera image settings
            }
          }
        }

     // if image gain was adjusted - cameraImageGain
        if (server.hasArg("gain")) {
          if (serialDebug) Serial.println("Gain has been changed");
          String Tvalue = server.arg("gain");   // read value
            if (Tvalue != NULL) {
              int val = Tvalue.toInt();
              if (val >= 0 && val <= 31 && val != cameraImageGain) {
                if (serialDebug) Serial.printf("Gain changed to %d\n", val);
                cameraImageGain = val;
                cameraImageSettings(0);          // Apply camera image settings
              }
            }
         }
  }


// ----------------------------------------------------------------
//       -root web page requested    i.e. http://x.x.x.x/
// ----------------------------------------------------------------
// web page with control buttons, links etc.

void handleRoot() {

 getNTPtime(2);                                             // refresh current time from NTP server
 WiFiClient client = server.client();                       // open link with client

 rootUserInput(client);                                     // Action any user input from this web page

 // html header
   sendHeader(client, stitle);
   client.write("<FORM action='/' method='post'>\n");            // used by the buttons in the html (action = the web page to send it to


 // --------------------------------------------------------------------

 // html main body
 //                    Info on the arduino ethernet library:  https://www.arduino.cc/en/Reference/Ethernet
 //                                            Info in HTML:  https://www.w3schools.com/html/
 //     Info on Javascript (can be inserted in to the HTML):  https://www.w3schools.com/js/default.asp
 //                               Verify your HTML is valid:  https://validator.w3.org/


  // ---------------------------------------------------------------------------------------------
  //  info which is periodically updated using AJAX - https://www.w3schools.com/xml/ajax_intro.asp

    // empty lines which are populated via vbscript with live data from http://x.x.x.x/data in the form of comma separated text
      int noLines = 6;      // number of text lines to be populated by javascript
      for (int i = 0; i < noLines; i++) {
        client.println("<span id='uline" + String(i) + "'></span><br>");
      }

    // Javascript - to periodically update the above info lines from http://x.x.x.x/data
    // Note: You can compact the javascript to save flash memory via https://www.textfixer.com/html/compress-html-compression.php
    //       The below = client.printf(R"=====(  <script> function getData() { var xhttp = new XMLHttpRequest(); xhttp.onreadystatechange = function() { if (this.readyState == 4 && this.status == 200) { var receivedArr = this.responseText.split(','); for (let i = 0; i < receivedArr.length; i++) { document.getElementById('uline' + i).innerHTML = receivedArr[i]; } } }; xhttp.open('GET', 'data', true); xhttp.send();} getData(); setInterval(function() { getData(); }, %d); </script> )=====", dataRefresh * 1000);

      // get a comma seperated list from http://x.x.x.x/data and populate the blank lines in html above
      client.printf(R"=====(
         <script>
            function getData() {
              var xhttp = new XMLHttpRequest();
              xhttp.onreadystatechange = function() {
              if (this.readyState == 4 && this.status == 200) {
                var receivedArr = this.responseText.split(',');
                for (let i = 0; i < receivedArr.length; i++) {
                  document.getElementById('uline' + i).innerHTML = receivedArr[i];
                }
              }
            };
            xhttp.open('GET', 'data', true);
            xhttp.send();}
            getData();
            setInterval(function() { getData(); }, %d);
         </script>
      )=====", dataRefresh * 1000);


  // ---------------------------------------------------------------------------------------------


//    // touch input on the two gpio pins
//      client.printf("<p>Touch on pin 12: %d </p>\n", touchRead(T5) );
//      client.printf("<p>Touch on pin 13: %d </p>\n", touchRead(T4) );

   // OTA
      if (OTAEnabled) client.write("<br>OTA IS ENABLED!"); 

   // Control buttons
     client.write("<br><br>");
     client.write("<input style='height: 35px;' name='button1' value='Toggle pin 12' type='submit'> \n");
     client.write("<input style='height: 35px;' name='button2' value='Cycle illumination LED' type='submit'> \n");
     client.write("<input style='height: 35px;' name='button3' value='Toggle Flash' type='submit'> \n");
     client.write("<input style='height: 35px;' name='button4' value='Wipe SPIFFS memory' type='submit'> \n");

  // change resolution
    //client.write("<br><label for='options'>Change camera resolution:</label>\n");
    client.write("<br><select id='resolution' name='resolution'>\n");
    client.write("  <option value='n/a'>Change camera resolution</option>\n");   // for if none selected
    client.write("  <option value='QVGA'>320x240 QVGA</option>\n");
    client.write("  <option value='VGA'>640x480 VGA</option>\n");
    client.write("  <option value='SVGA'>800x600 SVGA</option>\n");
    client.write("  <option value='XGA'>1024x768 XGA</option>\n");
    client.write("  <option value='SXGA'>1280x1024 SXGA</option>\n");
    client.write("</select>\n");

   // Image setting controls
     client.println("<br>CAMERA SETTINGS: ");
     client.printf("Brightness: <input type='number' style='width: 50px' name='bright' title='from -2 to +2' min='-2' max='2' value='%d'>  \n", cameraImageBrightness);
     client.printf("Exposure: <input type='number' style='width: 50px' name='exp' title='from 0 to 1200' min='0' max='1200' value='%d'>  \n", cameraImageExposure);
     client.printf("Gain: <input type='number' style='width: 50px' name='gain' title='from 0 to 30' min='0' max='30' value='%d'>\n", cameraImageGain);
     client.println(" <input type='submit' name='submit' value='Submit changes'>");
     client.println("<br>Set exposure and gain to zero for auto adjust");

   // links to the other pages available
     client.write("<br><br>LINKS: \n");
     client.write("<a href='/photo'>Store image</a> - \n");
     client.write("<a href='/img'>View stored image</a> - \n");
     client.write("<a href='/rgb'>RGB frame as data</a> - \n");
     client.write("<a href='/graydata'>Grayscale frame as data</a> \n");
     client.write("<br>");
     client.write("<a href='/stream'>Live stream</a> - \n");
     client.write("<a href='/jpg'>JPG</a> - \n");
     client.write("<a href='/jpeg'>Updating JPG</a> - \n");
     client.write("<a href='/test'>Test procedure</a>\n");
     #if ENABLE_OTA
        client.write(" - <a href='/ota'>Update via OTA</a>\n");
     #endif

    // addnl info if sd card present
     if (sdcardPresent) {
       client.write("<br>Note: You can view the individual stored images on sd card with:   http://x.x.x.x/img?img=1");
     }

    // capture and show a jpg image
      client.write("<br><br><a href='/jpg'>");         // make it a link
      client.write("<img id='image1' src='/jpg' width='320' height='240' /> </a>");     // show image from http://x.x.x.x/jpg

    // javascript to refresh the image periodically
      client.printf(R"=====(
         <script>
           function refreshImage(){
               var timestamp = new Date().getTime();
               var el = document.getElementById('image1');
               var queryString = '?t=' + timestamp;
               el.src = '/jpg' + queryString;
           }
           setInterval(function() { refreshImage(); }, %d);
         </script>
      )=====", imagerefresh * 1013);        // 1013 is just to stop it refreshing at the same time as /data

    client.println("<br><br><a href='https://github.com/alanesq/esp32cam-demo'>Sketch Info</a>");


 // --------------------------------------------------------------------


 sendFooter(client);     // close web page

}  // handleRoot


// ----------------------------------------------------------------
//     -data web page requested     i.e. http://x.x.x.x/data
// ----------------------------------------------------------------
// the root web page requests this periodically via Javascript in order to display updating information.
// information in the form of comma seperated text is supplied which are then inserted in to blank lines on the web page
// This makes it very easy to modify the data shown without having to change the javascript or root page html
// Note: to change the number of lines displayed update variable 'noLines' in handleroot()

void handleData(){

  // sd sdcard info
    uint32_t SDusedSpace = 0;
    uint32_t SDtotalSpace = 0;
    uint32_t SDfreeSpace = 0;
    if (sdcardPresent) {
      SDusedSpace = SD_MMC.usedBytes() / (1024 * 1024);
      SDtotalSpace = SD_MMC.totalBytes() / (1024 * 1024);
      SDfreeSpace = SDtotalSpace - SDusedSpace;
    }

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/plain", "");

  // line1 - sd card
    if (!sdcardPresent) {
      server.sendContent("<span style='color:blue;'>NO SD CARD DETECTED</span>");
    } else {
      server.sendContent("SD Card: " + String(SDusedSpace) + "MB used - " + String(SDfreeSpace) + "MB free");
    }
    server.sendContent(",");

  // line2 - illumination/flash led
    server.sendContent("Illumination led brightness=" + String(brightLEDbrightness));
    server.sendContent(" &ensp; Flash is ");     // Note: '&ensp;' leaves a gap
    server.sendContent( (flashRequired) ? "Enabled" : "Off" );
    server.sendContent(",");

  // line3 - Current real time
    server.sendContent("Current time: " + localTime());
    server.sendContent(",");

  // line4 - gpio pin status
    server.sendContent("GPIO output pin 12 is: ");
    server.sendContent( (digitalRead(iopinB)==1) ? "ON" : "OFF" );
    server.sendContent(" &ensp; GPIO input pin 13 is: ");
    server.sendContent( (digitalRead(iopinA)==1) ? "ON" : "OFF" );
    server.sendContent(",");

  // line5 - image resolution
    server.sendContent("Image size: " + ImageResDetails);
    server.sendContent(",");

  // line6 - memory - wifi
    server.sendContent("Free memory: " + String(ESP.getFreeHeap() /1000) + "K");
    if (!psramFound()) server.sendContent(" (No PSRAM found!)");
    server.sendContent("&ensp; Wifi strength: " + String(WiFi.RSSI()) + "dBm");

  // server.client().stop();
}


// ----------------------------------------------------------------
//    -photo save to sd card/spiffs    i.e. http://x.x.x.x/photo
// ----------------------------------------------------------------
// web page to capture an image from camera and save to spiffs or sd card

void handlePhoto() {

 WiFiClient client = server.client();                                                        // open link with client

 // log page request including clients IP
   IPAddress cIP = client.remoteIP();
   if (serialDebug) Serial.println("Save photo requested by " + cIP.toString());

 byte sRes = storeImage();   // capture and save an image to sd card or spiffs (store sucess or failed flag - 0=fail, 1=spiffs only, 2=spiffs and sd card)

 // html header
   sendHeader(client, "Capture and save image");

 // html body
   if (sRes == 2) {
       client.printf("<p>Image saved to sd card as image number %d </p>\n", imageCounter);
   } else if (sRes == 1) {
       client.write("<p>Image saved in Spiffs</p>\n");
   } else {
       client.write("<p>Error: Failed to save image</p>\n");
   }

   client.write("<a href='/'>Return</a>\n");       // link back

 // close web page
   sendFooter(client);

}  // handlePhoto



// ----------------------------------------------------------------
// -display image stored on sd card or SPIFFS   i.e. http://x.x.x.x/img?img=x
// ----------------------------------------------------------------
// Display a previously stored image, default image = most recent
// returns 1 if image displayed ok

bool handleImg() {

   WiFiClient client = server.client();                 // open link with client
   bool pRes = 0;

   // log page request including clients IP
     IPAddress cIP = client.remoteIP();
     if (serialDebug) Serial.println("Display stored image requested by " + cIP.toString());

   int imgToShow = imageCounter;                        // default to showing most recent file

   // get image number from url parameter
     if (server.hasArg("img") && sdcardPresent) {
       String Tvalue = server.arg("img");               // read value
       imgToShow = Tvalue.toInt();                      // convert string to int
       if (imgToShow < 1 || imgToShow > imageCounter) imgToShow = imageCounter;    // validate image number
     }

   // if stored on sd card
   if (sdcardPresent) {
     if (serialDebug) Serial.printf("Displaying image #%d from sd card", imgToShow);

     String tFileName = "/img/" + String(imgToShow) + ".jpg";
     fs::FS &fs = SD_MMC;                                 // sd card file system
     File timg = fs.open(tFileName, "r");
     if (timg) {
         size_t sent = server.streamFile(timg, "image/jpeg");     // send the image
         timg.close();
         pRes = 1;                                                // flag sucess
     } else {
       if (serialDebug) Serial.println("Error: image file not found");
       sendHeader(client, "Display stored image");
       client.write("<p>Error: Image not found</p></html>\n");
       client.write("<br><a href='/'>Return</a>\n");       // link back
       sendFooter(client);     // close web page
     }
   }

   // if stored in SPIFFS
   if (!sdcardPresent) {
     if (serialDebug) Serial.println("Displaying image from spiffs");

     // check file exists
     if (!SPIFFS.exists(spiffsFilename)) {
       sendHeader(client, "Display stored image");
       client.write("Error: No image found to display\n");
       client.write("<br><a href='/'>Return</a>\n");       // link back
       sendFooter(client);     // close web page
       return 0;
     }

     File f = SPIFFS.open(spiffsFilename, "r");                         // read file from spiffs
         if (!f) {
           if (serialDebug) Serial.printf("Error reading %s\n", spiffsFilename.c_str());
           sendHeader(client, "Display stored image");
           client.write("Error reading file from Spiffs\n");
           client.write("<br><a href='/'>Return</a>\n");       // link back
           sendFooter(client);     // close web page
         }
         else {
             size_t sent = server.streamFile(f, "image/jpeg");     // send file to web page
             if (!sent) {
               if (serialDebug) Serial.printf("Error sending %s\n", spiffsFilename.c_str());
             } else {
               pRes = 1;                                           // flag sucess
             }
             f.close();
         }
   }
   return pRes;

}  // handleImg


// ----------------------------------------------------------------
//                      -invalid web page requested
// ----------------------------------------------------------------
// Note: shows a different way to send the HTML reply

void handleNotFound() {

 String tReply;

 if (serialDebug) Serial.print("Invalid page requested");

 tReply = "File Not Found\n\n";
 tReply += "URI: ";
 tReply += server.uri();
 tReply += "\nMethod: ";
 tReply += ( server.method() == HTTP_GET ) ? "GET" : "POST";
 tReply += "\nArguments: ";
 tReply += server.args();
 tReply += "\n";

 for ( uint8_t i = 0; i < server.args(); i++ ) {
   tReply += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
 }

 server.send ( 404, "text/plain", tReply );
 tReply = "";      // clear variable

}  // handleNotFound


// ----------------------------------------------------------------
//      -access image data as RGB - i.e. http://x.x.x.x/rgb
// ----------------------------------------------------------------
//Demonstration on how to access raw RGB data from the camera
// Notes:
//     Set sendRGBfile to 1 in the settings at top of sketch to just send the raw rgb data as a file which can then be used with
//       the Processing sketch: https://github.com/alanesq/esp32cam-demo/blob/master/Misc/displayRGB.pde
//       otherwise a web page is displayed showing some sample rgb data usage.
//     You may want to disable auto white balance when experimenting with RGB otherwise the camera is always trying to adjust the
//        image colours to mainly white.   (disable in the 'cameraImageSettings' procedure).
//     It will fail on the higher resolutions as it requires more than the 4mb of available psram to store the data (1600x1200x3 bytes)
//     See this sketch for example of saving and viewing RGB files: https://github.com/alanesq/misc/blob/main/saveAndViewRGBfiles.ino
//     I learned how to read the RGB data from: https://github.com/Makerfabs/Project_Touch-Screen-Camera/blob/master/Camera_v2/Camera_v2.ino

void readRGBImage() {
                                                                                            // used for timing operations
 WiFiClient client = server.client();
 uint32_t tTimer;     // used to time tasks                                                                    // open link with client

 if (!sendRGBfile) {
   // html header
    sendHeader(client, "Show RGB data");

   // page title including clients IP
     IPAddress cIP = client.remoteIP();
     sendText(client, "Live image as rgb data, requested by " + cIP.toString());                                                            // 'sendText' sends the String to both serial port and web page
 }

 // make sure psram is available
 if (!psramFound()) {
   sendText(client,"error: no psram available to store the RGB data");
   client.write("<br><a href='/'>Return</a>\n");       // link back
   if (!sendRGBfile) sendFooter(client);               // close web page
   return;
 }


 //   ****** the main code for converting an image to RGB data *****

   // capture a live image from camera (as a jpg)
      tTimer = millis();                                                                                    // store time that image capture started
      camera_fb_t * fb = NULL;
      fb = esp_camera_fb_get();

     if (!fb) {
       sendText(client,"error: failed to capture image from camera");
       client.write("<br><a href='/'>Return</a>\n");       // link back
       if (!sendRGBfile) sendFooter(client);               // close web page
       return;
     } else {
       sendText(client, "JPG image capture took " + String(millis() - tTimer) + " milliseconds");              // report time it took to capture an image
       sendText(client, "Image resolution=" + String(fb->width) + "x" + String(fb->height));
       sendText(client, "Image size=" + String(fb->len) + " bytes");
       sendText(client, "Image format=" + String(fb->format));
       sendText(client, "Free memory=" + String(ESP.getFreeHeap()) + " bytes");
     }

/*
   // display captured image using base64 - seems a bit unreliable especially with larger images?
    if (!sendRGBfile) {
      client.print("<br>Displaying image direct from frame buffer");
      String base64data = base64::encode(fb->buf, fb->len);      // convert buffer to base64
      client.print(" - Base64 data length = " + String(base64data.length()) + " bytes\n" );
      client.print("<br><img src='data:image/jpg;base64," + base64data + "'></img><br>\n");
    }
*/

   // allocate memory to store the rgb data (in psram, 3 bytes per pixel)
     sendText(client,"<br>Free psram before rgb data allocated = " + String(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024) + "K");
     void *ptrVal = NULL;                                                                                 // create a pointer for memory location to store the data
     uint32_t ARRAY_LENGTH = fb->width * fb->height * 3;                                                  // calculate memory required to store the RGB data (i.e. number of pixels in the jpg image x 3)
     if (heap_caps_get_free_size( MALLOC_CAP_SPIRAM) <  ARRAY_LENGTH) {
       sendText(client,"error: not enough free psram to store the rgb data");
       if (!sendRGBfile) {
         client.write("<br><a href='/'>Return</a>\n");    // link back
         sendFooter(client);                              // close web page
       }
       return;
     }
     ptrVal = heap_caps_malloc(ARRAY_LENGTH, MALLOC_CAP_SPIRAM);                                          // allocate memory space for the rgb data
     uint8_t *rgb = (uint8_t *)ptrVal;                                                                    // create the 'rgb' array pointer to the allocated memory space
     sendText(client,"Free psram after rgb data allocated = " + String(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024) + "K");

   // convert the captured jpg image (fb) to rgb data (store in 'rgb' array)
      tTimer = millis();                                                                                   // store time that image conversion process started
      bool jpeg_converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb);
      if (!jpeg_converted) {
          sendText(client,"error: failed to convert image to RGB data");
          heap_caps_free(ptrVal);   
          esp_camera_fb_return(fb);   
          if (!sendRGBfile) {
              client.write("<br><a href='/'>Return</a>\n");    // link back
              sendFooter(client);                              // close web page
          }
          return;
      }
     sendText(client, "Conversion from jpg to RGB took " + String(millis() - tTimer) + " milliseconds");// report how long the conversion took


   // if sendRGBfile is set then just send raw RGB data and close
   if (sendRGBfile) {
     client.write(rgb, ARRAY_LENGTH);          // send the raw rgb data
     esp_camera_fb_return(fb);                 // camera frame buffer
     delay(3);
     client.stop();
     return;
   }

 //   ****** examples of using the resulting RGB data *****

   // display some of the resulting data
       uint32_t resultsToShow = 50;                                                                       // how much data to display
       sendText(client,"<br>R,G,B data for first " + String(resultsToShow / 3) + " pixels of image");
       for (uint32_t i = 0; i < resultsToShow-2; i+=3) {
         sendText(client,String(rgb[i+2]) + "," + String(rgb[i+1]) + "," + String(rgb[i+0]));           // Red , Green , Blue
         // // calculate the x and y coordinate of the current pixel
         //   uint16_t x = (i / 3) % fb->width;
         //   uint16_t y = floor( (i / 3) / fb->width);
       }

   // find the average values for each colour over entire image
       uint32_t aRed = 0;
       uint32_t aGreen = 0;
       uint32_t aBlue = 0;
       for (uint32_t i = 0; i < (ARRAY_LENGTH - 2); i+=3) {                                               // go through all data and add up totals
         aBlue+=rgb[i];
         aGreen+=rgb[i+1];
         aRed+=rgb[i+2];
       }
       aRed = aRed / (fb->width * fb->height);                                                            // divide total by number of pixels to give the average value
       aGreen = aGreen / (fb->width * fb->height);
       aBlue = aBlue / (fb->width * fb->height);
       sendText(client,"Average Blue = " + String(aBlue));
       sendText(client,"Average Green = " + String(aGreen));
       sendText(client,"Average Red = " + String(aRed));
       sendText(client,"Image luminance = " + String( aRed * 0.3 + aGreen * 0.59 + aBlue * 0.11 ) );


 //   *******************************************************

 client.write("<br><a href='/'>Return</a>\n");       // link back
 sendFooter(client);     // close web page

 // finished with the data so free up the memory space used in psram
   esp_camera_fb_return(fb);   // camera frame buffer
   heap_caps_free(ptrVal);     // rgb data

}  // readRGBImage


// ----------------------------------------------------------------
//                      -get time from ntp server
// ----------------------------------------------------------------

bool getNTPtime(int sec) {

   uint32_t start = millis();      // timeout timer

   do {
     time(&now);
     localtime_r(&now, &timeinfo);
     if (serialDebug) Serial.print(".");
     delay(100);
   } while (((millis() - start) <= (1000 * sec)) && (timeinfo.tm_year < (2016 - 1900)));

   if (timeinfo.tm_year <= (2016 - 1900)) return false;  // the NTP call was not successful
   if (serialDebug) {
     Serial.print("now ");
     Serial.println(now);
   }

   // Display time
   if (serialDebug)  {
     char time_output[30];
     strftime(time_output, 30, "%a  %d-%m-%y %T", localtime(&now));
     Serial.println(time_output);
     Serial.println();
   }
 return true;
}


// ----------------------------------------------------------------
//     -capture jpg image and send    i.e. http://x.x.x.x/jpg
// ----------------------------------------------------------------

bool handleJPG() {
    WiFiClient client = server.client();          
    char buf[32];

    camera_fb_t * fb = NULL;

    // drop first frame to ensure it is not an old image
      fb = esp_camera_fb_get();
      esp_camera_fb_return(fb);

    for (int attempts = 0; attempts < 3; attempts++) {
        fb = esp_camera_fb_get();
        if (fb && fb->buf && fb->len > 0) break;  // frame has been captured ok
        if (fb) esp_camera_fb_return(fb);
        fb = NULL;
        delay(60);                                // delay before retry
    }

    if (!fb) {
        if (serialDebug) Serial.println("Error: failed to capture image after retries");
        return 0;
    }

    ImageResDetails = String(fb->width) + "x" + String(fb->height);

    const char HEADER[] = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\n";
    const char CTNTTYPE[] = "Content-Type: image/jpeg\r\nContent-Length: ";
    client.write(HEADER, strlen(HEADER));
    client.write(CTNTTYPE, strlen(CTNTTYPE));

    sprintf(buf, "%d\r\n\r\n", fb->len);
    client.write(buf, strlen(buf));

    if (client.connected()) {
        client.write((char *)fb->buf, fb->len);
    } else {
        if (serialDebug) Serial.println("Error: client disconnected during transmission");
    }

    delay(3);
    client.clear();
    client.stop();

    esp_camera_fb_return(fb);
    fb = NULL;

    return 1;
}


// ----------------------------------------------------------------
//      -stream requested     i.e. http://x.x.x.x/stream
// ----------------------------------------------------------------
// Sends video stream - thanks to Uwe Gerlach for the code showing me how to do this

void handleStream(){

  WiFiClient client = server.client();          // open link with client
  char buf[32];
  camera_fb_t * fb = NULL;

  // log page request including clients IP
    IPAddress cIP = client.remoteIP();
    if (serialDebug) Serial.println("Live stream requested by " + cIP.toString());

 // html
 const char HEADER[] = "HTTP/1.1 200 OK\r\n" \
                       "Access-Control-Allow-Origin: *\r\n" \
                       "Content-Type: multipart/x-mixed-replace; boundary=123456789000000000000987654321\r\n";
 const char BOUNDARY[] = "\r\n--123456789000000000000987654321\r\n";           // marks end of each image frame
 const char CTNTTYPE[] = "Content-Type: image/jpeg\r\nContent-Length: ";       // marks start of image data
 const int hdrLen = strlen(HEADER);         // length of the stored text, used when sending to web page
 const int bdrLen = strlen(BOUNDARY);
 const int cntLen = strlen(CTNTTYPE);
 client.write(HEADER, hdrLen);
 client.write(BOUNDARY, bdrLen);

// The original while loop block
while (true)
{
  // 1. Check for excessive temperature
  float currentTemp = temperatureRead();
  if (currentTemp > MAX_TEMP_C) {
    if (serialDebug) {
      Serial.print("⚠️ WARNING: Overheating detected! Temperature: ");
      Serial.print(currentTemp);
      Serial.println("°C. Stopping stream.");
    }
    // Perform any necessary cleanup before breaking (optional)
    // You might want to close the client connection here if it's open, but
    // since you're breaking out of the loop, the main code will handle it.
    break; // Exit the while loop to stop the streaming/capture process
  }

  // 2. Check client connection (original check)
  if (!client.connected()) break;

  // 3. Image Capture and Sending (original code)
  fb = esp_camera_fb_get(); // capture live image as jpg
  if (!fb) {
    if (serialDebug) Serial.println("Error: failed to capture jpg image");
  } else {
    // send image
    client.write(CTNTTYPE, cntLen);             // send content type html (i.e. jpg image)
    sprintf( buf, "%d\r\n\r\n", fb->len);       // format the image's size as html and put in to 'buf'
    client.write(buf, strlen(buf));             // send result (image size)
    client.write((char *)fb->buf, fb->len);     // send the image data
    client.write(BOUNDARY, bdrLen);             // send html boundary      
    esp_camera_fb_return(fb);                   // return camera frame buffer
  }
}

 if (serialDebug) Serial.println("Video stream stopped");
 delay(3);
 client.stop();


}  // handleStream


// ----------------------------------------------------------------
//                        request a web page
// ----------------------------------------------------------------
//   @param    page         web page to request
//   @param    received     String to store response in
//   @param    maxWaitTime  maximum time to wait for reply (ms)
//   @returns  http code
// see:  https://randomnerdtutorials.com/esp32-http-get-post-arduino/#http-get-1
// to do:  limit size of reply

int requestWebPage(String* page, String* received, int maxWaitTime=5000){

  if (serialDebug) Serial.println("requesting web page: " + *page);

  WiFiClient client;
  HTTPClient http;     // see:  https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient
  http.setTimeout(maxWaitTime);
  http.begin(client, *page);      // for https requires (client, *page, thumbprint)  e.g.String thumbprint="08:3B:71:72:02:43:6E:CA:ED:42:86:93:BA:7E:DF:81:C4:BC:62:30";
  int httpCode = http.GET();      // http codes: https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
  if (serialDebug) Serial.println("http code: " + String(httpCode));

  if (httpCode > 0) {
    *received = http.getString();
  } else {
    *received = "error:" + String(httpCode);
  }
  if (serialDebug) Serial.println(*received);

  http.end();   //Close connection
  if (serialDebug) Serial.println("Web connection closed");

  return httpCode;

}  // requestWebPage


// ----------------------------------------------------------------
//       -show refreshing image    i.e. http://x.x.x.x/jpeg
// ----------------------------------------------------------------

void handleJpeg() {

  const int refreshRate = 2000;     // image refresh rate (ms)

  WiFiClient client = server.client();                 // open link with client

    // Start page
      client.write("HTTP/1.1 200 OK\r\n");
      client.write("Content-Type: text/html\r\n");
      client.write("Connection: close\r\n");
      client.write("\r\n");
      client.write("<!DOCTYPE HTML><html lang='en'>\n");
      client.write("<head></head><body>");

    client.write("<FORM action='/' method='post'>\n");            // used by the buttons in the html (action = the web page to send it to

    // capture and show a jpg image
      client.write("<img id='image1' src='/jpg'/>");     // show image from http://x.x.x.x/jpg

    // javascript to refresh the image periodically
      client.printf(R"=====(
         <script>
           function refreshImage(){
               var timestamp = new Date().getTime();
               var el = document.getElementById('image1');
               var queryString = '?t=' + timestamp;
               el.src = '/jpg' + queryString;
           }
           setInterval(function() { refreshImage(); }, %d);
         </script>
      )=====", refreshRate);        


  sendFooter(client);     // close web page
}  // handleJpeg


// ----------------------------------------------------------------
//                     resize grayscale image
// ----------------------------------------------------------------
// Thanks to Bard A.I. for writing this for me ;-)
//   src_buf: The source image buffer.
//   src_width: The width of the source image buffer.
//   src_height: The height of the source image buffer.
//   dst_buf: The destination image buffer.
//   dst_width: The width of the destination image buffer.
//   dst_height: The height of the destination image buffer.
void resize_esp32cam_image_buffer(uint8_t* src_buf, int src_width, int src_height,
                                   uint8_t* dst_buf, int dst_width, int dst_height) {
  // Calculate the horizontal and vertical resize ratios.
  float h_ratio = (float)src_width / dst_width;
  float v_ratio = (float)src_height / dst_height;

  // Iterate over the destination image buffer and write the resized pixels.
  for (int y = 0; y < dst_height; y++) {
    for (int x = 0; x < dst_width; x++) {
      // Calculate the source pixel coordinates.
      int src_x = (int)(x * h_ratio);
      int src_y = (int)(y * v_ratio);

      // Read the source pixel value.
      uint8_t src_pixel = src_buf[src_y * src_width + src_x];

      // Write the resized pixel value to the destination image buffer.
      dst_buf[y * dst_width + x] = src_pixel;
    }
  }
}


// ----------------------------------------------------------------
//                  Capture grayscale image data
// ----------------------------------------------------------------

void readGrayscaleImage() {

  WiFiClient client = server.client();                 // open link with client

  // html header
   sendHeader(client, "Access grayscale image data");  

  // change camera to grayscale mode  (by default it is in JPG colour mode)
    esp_camera_deinit();                           // disable camera
    delay(camChangeDelay);
    config.pixel_format = PIXFORMAT_GRAYSCALE;     // change camera setting to grayscale (default is JPG)
    initialiseCamera(0);                           // restart the camera (0 = without resetting all the other camera settings)
    cameraImageSettings(1);

  // capture the image and use flash if required
    int currentBrightness = brightLEDbrightness;
    if (flashRequired) {
        analogWrite(brightLED, 255);   // change LED brightness (0 - 255)
        delay(100);
    }
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get();

    if (flashRequired){
        delay(100);
        analogWrite(brightLED, currentBrightness);            // change LED brightness back to previous state
    }
    if (!fb) {
        client.println("Error: Camera image capture failed");
        // Ensure we switch back even on failure
        esp_camera_deinit();
        delay(camChangeDelay);
        initialiseCamera(1);
        sendFooter(client); // Close page if needed
        return; // Exit
    }

  // // read image data and calculate average pixel value (as demonstration of reading the image data)
  // //      note:   image x = i % WIDTH, image y = floor(i / WIDTH)
  //   unsigned long dataSize = fb->width * fb->height;
  //   byte minV=255; byte maxV=0;
  //   for (int y=0; y < fb->height; y++) {
  //     for (int x=0; x < fb->width; x++) {   
  //       byte pixelVal = fb->buf[(y * fb->width) + x];
  //       if (pixelVal > maxV) maxV = pixelVal;
  //       if (pixelVal < minV) minV = pixelVal;
  //     }
  //   }
  //   client.print("Grayscale image: The lowest value pixel is " + String(minV) + ", the highest is " + String(maxV));  
  //   client.print("<br>");

  // resize the image
    int newWidth = 115;   int newHeight = 42;         // much bigger than this seems to cause problems, possible web page is too large?
    size_t newBufSize = newWidth * newHeight;
    byte* newBuf = (byte*)malloc(newBufSize); 
    if (!newBuf) {
        client.println("Error: Failed to allocate memory for resized buffer");
        esp_camera_fb_return(fb); // Clean up camera buffer
        // Switch back to JPG before returning
        esp_camera_deinit();
        delay(camChangeDelay);
        initialiseCamera(1);
        sendFooter(client); // Close page if needed
        return; // Exit
    }

    resize_esp32cam_image_buffer(fb->buf, fb->width, fb->height, newBuf, newWidth, newHeight);

  // Get the min and max values in the new resized image
    byte newminV=255; byte newmaxV=0;
    for (int y=0; y < newHeight; y++) {
      for (int x=0; x < newWidth; x++) {   
        byte pixelVal = newBuf[(y * newWidth) + x];
        if (pixelVal > newmaxV) newmaxV = pixelVal;
        if (pixelVal < newminV) newminV = pixelVal;
      }
    }
    client.printf("Resized image: The lowest value pixel is %d, the highest is %d\n", newminV, newmaxV);
    client.write("<br><br><a href='/'>Return</a>\n");       // link back    

  // display image as asciiArt
    char asciiArt[] = {'@','#','S','%','?','*','+',';',':',',','.',' ',' '};       // characters to use 
    int noAsciiChars = sizeof(asciiArt) / sizeof(asciiArt[0]);                 // number of characters available
    client.write("<br><pre style='line-height: 1.1;'>");                       // 'pre' stops variable character spacing, 'line-heigh' adjusts spacing between lines - 0.2
    for (int y=0; y < newHeight; y++) {
      client.write("\n");                                                      // new line
      for (int x=0; x < newWidth; x++) {   
        int tpos = map(newBuf[y*newWidth+x], newminV, newmaxV, 0, noAsciiChars - 1); // convert pixel brightness to ascii character  
        client.write(asciiArt[tpos]); 
      }
    }
    client.write("</pre><br>");

  // close web page
    sendFooter(client);    
    
  // close network client connection
    delay(3);
    client.stop();
    
  // free up memory
    free(newBuf);
    esp_camera_fb_return(fb);                 // return camera frame buffer

  // change camera back to JPG mode
    esp_camera_deinit();  
    delay(camChangeDelay);
    initialiseCamera(1);    // reset settings (1=apply the cameras settings which includes JPG mode)
  }


// ----------------------------------------------------------------
//   -reboot web page requested        i.e. http://x.x.x.x/reboot
// ----------------------------------------------------------------
// note: this can fail if the esp has just been reflashed and not restarted

void handleReboot(){

      String message = "Rebooting....";
      server.send(200, "text/plain", message);   // send reply as plain text

      // rebooting
        delay(500);          // give time to send the above html
        ESP.restart();
        delay(5000);         // restart fails without this delay
}


// ----------------------------------------------------------------
//   -switch GPIO pin via URL    i.e. http://x.x.x.x/switch?on=1
// ----------------------------------------------------------------

void handleSwitch() {

  WiFiClient client = server.client();             // open link with client

  String reply = "error - no command received";    // default message

    // switch gpio pin
      if (server.hasArg("on")) {   
          String Tvalue = server.arg("on");        // read value
          if (Tvalue != NULL) {
            int val = Tvalue.toInt();        
            if (val == 0) {
              digitalWrite(iopinB, LOW);
              reply = "Switched off";
            }
            if (val == 1) {
              digitalWrite(iopinB, HIGH);
              reply = "Switched on";
            }
          }
      }     
  
  server.send(200, "text/plain", reply);           // send reply as plain "ok"
}      


// ----------------------------------------------------------------
//      -ping web page requested     i.e. http://x.x.x.x/ping
// ----------------------------------------------------------------

void handlePing() {

  WiFiClient client = server.client();         // open link with client
  
  server.send(200, "text/plain", "ok");        // send reply as plain "ok"
}        


// ----------------------------------------------------------------
//       -test procedure    i.e. http://x.x.x.x/test
// ----------------------------------------------------------------

void handleTest() {

 WiFiClient client = server.client();                                                        // open link with client

 // log page request including clients IP
   IPAddress cIP = client.remoteIP();
   if (serialDebug) Serial.println("Test page requested by " + cIP.toString());

 // html header
   sendHeader(client, "Testing");


 // html body


 // -------------------------------------------------------------------




                          // test code goes here





// // demo of drawing on the camera image using javascript / html canvas
// //   could be of use to show area of interest on the image etc. - see https://www.w3schools.com/html/html5_canvas.asp
// // creat a DIV and put image in it with a html canvas on top of it
//   int imageWidth = 640;   // image dimensions on web page
//   int imageHeight = 480;
//   client.println("<div style='display:inline-block;position:relative;'>");
//   client.println("<img style='position:absolute;z-index:10;' src='/jpg' width='" + String(imageWidth) + "' height='" + String(imageHeight) + "' />");
//   client.println("<canvas style='position:relative;z-index:20;' id='myCanvas' width='" + String(imageWidth) + "' height='" + String(imageHeight) + "'></canvas>");
//   client.println("</div>");
// // javascript to draw on the canvas
//   client.println("<script>");
//   client.println("var imageWidth = " + String(imageWidth) + ";");
//   client.println("var imageHeight = " + String(imageHeight) + ";");
//   client.print (R"=====(
//     // connect to the canvas
//       var c = document.getElementById("myCanvas");
//       var ctx = c.getContext("2d");
//       ctx.strokeStyle = "red";
//     // draw on image
//       ctx.rect(imageWidth / 2, imageHeight / 2, 60, 40);                              // box
//       ctx.moveTo(20, 20); ctx.lineTo(200, 100);                                       // line
//       ctx.font = "30px Arial";  ctx.fillText("Hello World", 50, imageHeight - 50);    // text
//       ctx.stroke();
//    </script>\n)=====");


// // flip image horizontally
//   sensor_t *s = esp_camera_sensor_get();
//   s->set_hmirror(s, 1);


/*
 // demo of how to request a web page
   String page = "http://urlhere.com";   // url to request
   String response;                             // reply will be stored here
   int httpCode = requestWebPage(&page, &response);
   // show results
     client.println("Web page requested: '" + page + "' - http code: " + String(httpCode));
     client.print("<xmp>'");     // enables the html code to be displayed
     client.print(response);
     client.println("'</xmp><br>");
*/


/*
//  // demo useage of the mcp23017 io chipnote: this stops PWM on the flash working for some reason
    #if useMCP23017 == 1
      while(1) {
          mcp.digitalWrite(0, HIGH);
          int q = mcp.digitalRead(8);
          client.print("<p>HIGH, input =" + String(q) + "</p>");
          delay(1000);
          mcp.digitalWrite(0, LOW);
          client.print("<p>LOW</p>");
          delay(1000);
      }
    #endif
*/


 // -------------------------------------------------------------------

 client.println("<br><br><a href='/'>Return</a>");       // link back
 sendFooter(client);     // close web page

}  // handleTest


// ******************************************************************************************************************
// end
