 /*******************************************************************************************************************
 *            
 *                                 ESP32Cam development board demo sketch using Arduino IDE
 *                                    Github: https://github.com/alanesq/ESP32Cam-demo
 *     
 *     Starting point sketch for projects using the esp32cam development board with the following features
 *        web server with live video streaming and RGB data from camera demonstrated.
 *        sd card support using 1-bit mode (data pins are usually 2,4,12&13 but using 1bit mode only uses pin 2)
 *        flash led is still available for use (pin 4) and does not flash when accessing sd card
 *        Stores image in Spiffs if no sd card present
 *        PWM control of the illumination/flash LED
 * 
 *     GPIO:
 *        You can use io pins 13 and 12 for input or output (but 12 must not be high at boot)
 *        pin 16 is used for psram but you may get away with using it as input for a button etc.
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
 *            
 *     To see a more advanced sketch along the same format as this one have a look at https://github.com/alanesq/CameraWifiMotion
 *        which includes email support, FTP, OTA updates and motion detection
 * 
 * 
 *     esp32cam-demo is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the 
 *        implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * 
 *******************************************************************************************************************/

 #if !defined ESP32
  #error This sketch is only for an ESP32Cam module
#endif

#include "esp_camera.h"       // https://github.com/espressif/esp32-camera


// ******************************************************************************************************************




//   ---------------------------------------------------------------------------------------------------------

//                                      Wifi Settings

#include <wifiSettings.h>       // delete this line, un-comment the below two lines and enter your wifi details

//const char *SSID = "your_wifi_ssid";

//const char *PWD = "your_wifi_pwd";


//   ---------------------------------------------------------------------------------------------------------




// ---------------------------------------------------------------
//                           -SETTINGS
// ---------------------------------------------------------------

  const char* stitle = "ESP32Cam-demo";                  // title of this sketch
  const char* sversion = "15Apr21";                      // Sketch version

  bool sendRGBfile = 0;                                  // if set '/rgb' will send the rgb data as a file rather than display some on a HTML page

  const bool serialDebug = 1;                            // show info. on serial port (1=enabled, disable if using pins 1 and 3 as gpio)

  #define useMCP23017 0                                  // if MCP23017 IO expander chip is being used (on pins 12 and 13)

  // Camera related
    const bool flashRequired = 1;                        // If flash to be used when capturing image (1 = yes)
    const framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_VGA;  // Image resolution:   
                                                         //               default = "const framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_VGA"
                                                         //               160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA), 
                                                         //               320x240 (QVGA), 400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA), 
                                                         //               1024x768 (XGA), 1280x1024 (SXGA), 1600x1200 (UXGA)
    int cameraImageExposure = 0;                         // Camera exposure (0 - 1200)   If gain and exposure both set to zero then auto adjust is enabled
    int cameraImageGain = 0;                             // Image gain (0 - 30)

  const int TimeBetweenStatus = 600;                     // speed of flashing system running ok status light (milliseconds)

  const int indicatorLED = 33;                           // onboard small LED pin (33)

  const int brightLED = 4;                               // onboard Illumination/flash LED pin (4)

  const int iopinA = 13;                                 // general io pin 13
  const int iopinB = 12;                                 // general io pin 12 (must not be high at boot)
  const int iopinC = 16;                                 // input only pin 16 (used by PSRam but you may get away with using it for a button)
  
  const int serialSpeed = 115200;                        // Serial data speed to use

  // NTP - Internet time
    const char* ntpServer = "pool.ntp.org";
    const char* TZ_INFO    = "GMT+0BST-1,M3.5.0/01:00:00,M10.5.0/02:00:00";  // enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
    long unsigned lastNTPtime;
    tm timeinfo;
    time_t now;
    
// camera settings (for the standard - OV2640 - CAMERA_MODEL_AI_THINKER)
//     see: https://randomnerdtutorials.com/esp32-cam-camera-pin-gpios/
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

  camera_config_t config;     


  
// ******************************************************************************************************************


#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>       // used by requestWebPage()
#include "driver/ledc.h"      // used to configure pwm on illumination led

// spiffs used to store images without an sd card
  #include <SPIFFS.h>
  #include <FS.h>             // gives file access on spiffs

WebServer server(80);                       // serve web pages on port 80

// Used to disable brownout detection 
  #include "soc/soc.h"                       
  #include "soc/rtc_cntl_reg.h"      

// sd-card
  #include "SD_MMC.h"                         // sd card - see https://randomnerdtutorials.com/esp32-cam-take-photo-save-microsd-card/
  #include <SPI.h>                       
  #include <FS.h>                             // gives file access 
  #define SD_CS 5                             // sd chip select pin = 5

// MCP23017 IO expander on pins 12 and 13 (optional)
  #if useMCP23017 == 1
    #include <Wire.h>
    #include "Adafruit_MCP23017.h"
    Adafruit_MCP23017 mcp;
    // Wire.setClock(1700000); // set frequency to 1.7mhz
  #endif
  
// Define some global variables:
  uint32_t lastStatus = millis();           // last time status light changed status (to flash all ok led)
  uint32_t lastCamera = millis();           // timer for periodic image capture
  bool sdcardPresent;                       // flag if an sd card is detected
  int imageCounter;                         // image file name on sd card counter
  uint32_t illuminationLEDstatus;           // current brightness setting of the illumination led
  String spiffsFilename = "/image.jpg";     // image name to use when storing in spiffs
 
  
  
// ******************************************************************************************************************


// ---------------------------------------------------------------
//    -SETUP     SETUP     SETUP     SETUP     SETUP     SETUP
// ---------------------------------------------------------------

void setup() {
  
  if (serialDebug) {
    Serial.begin(serialSpeed);                     // Start serial communication 
  
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
      Serial.print(SSID);
      Serial.print("\n   ");
    }
    WiFi.begin(SSID, PWD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        if (serialDebug) Serial.print(".");
    }
    if (serialDebug) {
      Serial.print("\nWiFi connected, ");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
    server.begin();                               // start web server
    digitalWrite(indicatorLED,HIGH);              // small indicator led off

  // define the web pages (i.e. call these procedures when url is requested)
    server.on("/", handleRoot);                   // root page
    server.on("/stream", handleStream);           // stream live video
    server.on("/photo", handlePhoto);             // save image to sd card
    server.on("/img", handleImg);                 // show image from sd card
    server.on("/rgb", readRGBImage);              // demo converting image to RGB
    server.on("/test", handleTest);               // Testing procedure
    server.onNotFound(handleNotFound);            // invalid url requested

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
      if (setupCameraHardware()) {
        if (serialDebug) Serial.println("OK");
      }
      else {
        if (serialDebug) Serial.println("Error!");
        showError(2);                              // critical error so stop and flash led
      }

  // Spiffs - for storing images without an sd card
  //       see: https://circuits4you.com/2018/01/31/example-of-esp8266-flash-file-system-spiffs/
    if (!SPIFFS.begin(true)) {
      if (serialDebug) Serial.println(("An Error has occurred while mounting SPIFFS - restarting"));
      delay(5000);
      ESP.restart();                               // restart and try again
      delay(5000);
    } else {
      if (serialDebug) {
        Serial.print(("SPIFFS mounted successfully: "));
        Serial.printf("total bytes: %d , used: %d \n", SPIFFS.totalBytes(), SPIFFS.usedBytes());
      }
    }

  // SD Card - if one is detected set 'sdcardPresent' High
      if (!SD_MMC.begin("/sdcard", true)) {        // if loading sd card fails     
        // note: ('/sdcard", true)' = 1bit mode - see: https://www.reddit.com/r/esp32/comments/d71es9/a_breakdown_of_my_experience_trying_to_talk_to_an/
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
          if (serialDebug) Serial.printf("SD Card found, free space = %dMB \n", SDfreeSpace);  
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
    pinMode(iopinA, OUTPUT);                  // pin 13 - free io pin, can be used for input or output
    pinMode(iopinB, OUTPUT);                  // pin 12 - free io pin, can be used for input or output (must not be high at boot)
    pinMode(iopinC, INPUT);                   // pin 16 - free input only pin

  // check the esp32cam board has a psram chip installed (extra memory used for storing captured images)
  //    Note: if not using "AI thinker esp32 cam" in the Arduino IDE, SPIFFS must be enabled
  if (!psramFound()) {
    if (serialDebug) Serial.println("Warning: No PSRam found so defaulting to image size 'CIF'");
    framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_CIF;
  }

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

  illuminationSetup();             // configure PWM for the illumination/flash LED
    
  if (serialDebug) Serial.println("\nSetup complete...");

}  // setup



// ******************************************************************************************************************


// ----------------------------------------------------------------
//   -LOOP     LOOP     LOOP     LOOP     LOOP     LOOP     LOOP
// ----------------------------------------------------------------


void loop() {

  server.handleClient();          // handle any incoming web page requests





  
  //                           <<< YOUR CODE HERE >>>






//  //  demo to Capture an image and save to sd card every 5 seconds (i.e. time lapse)
//      if ( ((unsigned long)(millis() - lastCamera) >= 5000) && sdcardPresent ) { 
//        lastCamera = millis();     // reset timer
//        storeImage();              // save an image to sd card
//      }
 
  // flash status LED to show sketch is running ok
    if ((unsigned long)(millis() - lastStatus) >= TimeBetweenStatus) { 
      lastStatus = millis();                                               // reset timer
      digitalWrite(indicatorLED,!digitalRead(indicatorLED));               // flip indicator led status
    }
    
}  // loop



// ******************************************************************************************************************


// ----------------------------------------------------------------
//                        Configure the camera
// ----------------------------------------------------------------
// returns TRUE if sucessful

bool setupCameraHardware() {
  
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
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;               // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    config.pixel_format = PIXFORMAT_JPEG;         // Options =  YUV422, GRAYSCALE, RGB565, JPEG, RGB888
    config.frame_size = FRAME_SIZE_IMAGE;         // Image sizes: 160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA), 320x240 (QVGA), 
                                                  //              400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA), 1024x768 (XGA), 1280x1024 (SXGA), 
                                                  //              1600x1200 (UXGA)
    config.jpeg_quality = 5;                      // 0-63 lower number means higher quality
    config.fb_count = 1;                          // if more than one, i2s runs in continuous mode. Use only with JPEG
                 
    esp_err_t camerr = esp_camera_init(&config);  // initialise the camera
    if (camerr != ESP_OK) {
      if (serialDebug) Serial.printf("ERROR: Camera init failed with error 0x%x", camerr);
    }

    cameraImageSettings();                        // apply custom camera settings  
    
    return (camerr == ESP_OK);                    // return boolean result of camera initialisation
}


// ******************************************************************************************************************


// ----------------------------------------------------------------
//                   -Change camera image settings
// ----------------------------------------------------------------
// Adjust image properties (brightness etc.)
// Defaults to auto adjustments if exposure and gain are both set to zero
// - Returns TRUE if successful
// BTW - some interesting info on exposure times here: https://github.com/raduprv/esp32-cam_ov2640-timelapse

bool cameraImageSettings() { 
   
    sensor_t *s = esp_camera_sensor_get();       
    if (s == NULL) {
      if (serialDebug) Serial.println("Error: problem reading camera sensor settings");
      return 0;
    } 

    // if both set to zero enable auto adjust
    if (cameraImageExposure == 0 && cameraImageGain == 0) {              
      // enable auto adjust
        s->set_gain_ctrl(s, 1);                       // auto gain on 
        s->set_exposure_ctrl(s, 1);                   // auto exposure on 
        s->set_awb_gain(s, 1);                        // Auto White Balance enable (0 or 1)
    } else {
      // Apply manual settings
        s->set_gain_ctrl(s, 0);                       // auto gain off 
        s->set_awb_gain(s, 1);                        // Auto White Balance enable (0 or 1)
        s->set_exposure_ctrl(s, 0);                   // auto exposure off 
        s->set_agc_gain(s, cameraImageGain);          // set gain manually (0 - 30)
        s->set_aec_value(s, cameraImageExposure);     // set exposure manually  (0-1200)
    }

    return 1;
}  // cameraImageSettings


//    // More camera settings available:
//    // If you enable gain_ctrl or exposure_ctrl it will prevent a lot of the other settings having any effect
//    // more info on settings here: https://randomnerdtutorials.com/esp32-cam-ov2640-camera-settings/
//    s->set_gain_ctrl(s, 0);                       // auto gain off (1 or 0)
//    s->set_exposure_ctrl(s, 0);                   // auto exposure off (1 or 0)
//    s->set_agc_gain(s, cameraImageGain);          // set gain manually (0 - 30)
//    s->set_aec_value(s, cameraImageExposure);     // set exposure manually  (0-1200)
//    s->set_vflip(s, cameraImageInvert);           // Invert image (0 or 1)     
//    s->set_quality(s, 10);                        // (0 - 63)
//    s->set_gainceiling(s, GAINCEILING_32X);       // Image gain (GAINCEILING_x2, x4, x8, x16, x32, x64 or x128) 
//    s->set_brightness(s, cameraImageBrightness);  // (-2 to 2) - set brightness
//    s->set_lenc(s, 1);                            // lens correction? (1 or 0)
//    s->set_saturation(s, 0);                      // (-2 to 2)
//    s->set_contrast(s, cameraImageContrast);      // (-2 to 2)
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


// ******************************************************************************************************************


// ----------------------------------------------------------------
//             Set up pwm for the illumination led
// ----------------------------------------------------------------
// configure PWM for brightness control of the illumination led
// Note: Using this more long winded pwm setup as timer0 is already used by the camera (and channel 0)
//       more info: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html

void illuminationSetup() {

    // set up the timer
      ledc_timer_config_t timer_conf;
          timer_conf.duty_resolution = LEDC_TIMER_8_BIT;          // 8 bits gives a brightness range of 0 to 255
          timer_conf.freq_hz = 1000;                              // frequency of the pwm 
          timer_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
          timer_conf.timer_num = LEDC_TIMER_3;                    // which timer to use (0 to 3)
      ledc_timer_config(&timer_conf);

    // set up the channel
      ledc_channel_config_t ledc_conf;
          ledc_conf.channel = LEDC_CHANNEL_5;                     // led channel to use (0 to 15)
          // ledc_conf.duty = 0;                                     // 0=off, 255=fully on
          ledc_conf.gpio_num = brightLED;                         // gpio pin
          ledc_conf.intr_type = LEDC_INTR_DISABLE;
          ledc_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
          ledc_conf.timer_sel = LEDC_TIMER_3;                     // timer to use (0 to 3)
      ledc_channel_config(&ledc_conf);

    illuminationBrightness(0);                                    // set brightness to off
    
} // illuminationSetup


// ******************************************************************************************************************


// ----------------------------------------------------------------
//                        Misc small procedures
// ----------------------------------------------------------------


//  Set the illumination LED brightness (PWM)
void illuminationBrightness(uint32_t duty) {
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_5, duty);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_5);
        illuminationLEDstatus = duty;     // store current brightness
}


// returns the current real time as a String
//   see: https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
String localTime() {
  struct tm timeinfo;
  char ttime[40];
  if(!getLocalTime(&timeinfo)) return"Failed to obtain time";
  strftime(ttime,40,  "%A, %B %d %Y %H:%M:%S", &timeinfo);
  return ttime;
}


// flash the indicator led 'reps' number of times
void flashLED(int reps) {
  for(int x=0; x < reps; x++) {
    digitalWrite(indicatorLED,LOW);
    delay(1000);
    digitalWrite(indicatorLED,HIGH);
    delay(500);
  }
}


// critical error - stop sketch and continually flash error code on indicator led
void showError(int errorNo) {
  while(1) {
    flashLED(errorNo);
    delay(4000);
  }
}


// ******************************************************************************************************************


// ----------------------------------------------------------------
//     Capture image from camera and save to spiffs or sd card
// ----------------------------------------------------------------
// returns 0 if failed, 1 if stored in spiffs, 2 if stored on sd card

byte storeImage() {

  fs::FS &fs = SD_MMC;                              // sd card file system

  // capture the image from camera
    int currentBrightness = illuminationLEDstatus;
    if (flashRequired) illuminationBrightness(255);   // turn flash on
    camera_fb_t *fb = esp_camera_fb_get();            // capture image frame from camera
    if (flashRequired) illuminationBrightness(currentBrightness);     // return flash to previous state
    if (!fb) {
      if (serialDebug) Serial.println("Error: Camera capture failed");
      flashLED(3);   // stop and display error code on LED
      // return 0
    }

  // save image to Spiffs
    if (!sdcardPresent) {
      if (serialDebug) Serial.println("Storing image to spiffs only");
      SPIFFS.remove(spiffsFilename);                       // delete old image file if it exists
      File file = SPIFFS.open(spiffsFilename, FILE_WRITE);
      if (!file) {
        if (serialDebug) Serial.println("Failed to create file in Spiffs");
        return 0;
      }
      else {
        if (file.write(fb->buf, fb->len)) {  
          if (serialDebug)  {
            Serial.print("The picture has been saved as " + spiffsFilename);
            Serial.print(" - Size: ");
            Serial.print(file.size());
            Serial.println(" bytes");
          }
        } else {
          if (serialDebug) Serial.println("Error: writing image to spiffs...will format and try again");
          if (!SPIFFS.format()) {
            if (serialDebug) Serial.println("Error: Unable to format Spiffs"); 
            return 0;
          }
          file = SPIFFS.open(spiffsFilename, FILE_WRITE);
          if (!file.write(fb->buf, fb->len)) {
            if (serialDebug) Serial.println("Error: Still unable to write image to Spiffs");
            return 0;
          }
        }
      file.close();      
      }
    }
  
  // save the image to sd card
    if (sdcardPresent) {
      if (serialDebug) Serial.printf("Storing image #%d to sd card \n", imageCounter);
      String SDfilename = "/img/" + String(imageCounter + 1) + ".jpg";              // build the image file name
      File file = fs.open(SDfilename, FILE_WRITE);                                  // create file on sd card
      if (!file) {
        if (serialDebug) Serial.println("Error: Failed to create file on sd-card: " + SDfilename);
        flashLED(4);    // stop and display error code on LED
        // return 0
      } else {
        if (file.write(fb->buf, fb->len)) {                                         // File created ok so save image to it
          if (serialDebug) Serial.println("Image saved to sd card"); 
          imageCounter ++;                                                          // increment image counter
        } else {
          if (serialDebug) Serial.println("Error: failed to save image to sd card");
          flashLED(4);   // stop and display error code on LED
          // return 0;
        }
        file.close();              // close image file on sd card
      }
    }
    
  esp_camera_fb_return(fb);        // return frame so memory can be released
  
  if (sdcardPresent) return 2;     // saved to sd card 
  else return 1;                   // saved in spiffs

} // storeImage



// ******************************************************************************************************************


// ----------------------------------------------------------------
//       -root web page requested    i.e. http://x.x.x.x/
// ----------------------------------------------------------------
// web page with control buttons, links etc.

void handleRoot() {

  getNTPtime(2);                                             // refresh current time from NTP server

  WiFiClient client = server.client();                       // open link with client

  // log the page request including clients IP address
    if (serialDebug) {
      IPAddress cip = client.remoteIP();
      Serial.printf("Root page requested from: %d.%d.%d.%d \n", cip[0], cip[1], cip[2], cip[3]);
    }


  // Action any button presses or settings entered on web page

    // if button1 was pressed (toggle io pin A)
    //        Note:  if using an input box etc. you would read the value with the command:    String Bvalue = server.arg("demobutton1"); 
      if (server.hasArg("button1")) {
        digitalWrite(iopinA,!digitalRead(iopinA));             // toggle output pin on/off
        if (serialDebug) Serial.println("Button 1 pressed");       
      }
  
    // if button2 was pressed (toggle io pin B)
      if (server.hasArg("button2")) {
        digitalWrite(iopinB,!digitalRead(iopinB));             // toggle output pin on/off
        if (serialDebug) Serial.println("Button 2 pressed");
      }
  
    // if button3 was pressed (toggle flash LED)
      if (server.hasArg("button3")) {
        if (illuminationLEDstatus == 0) illuminationBrightness(30);          // turn led on dim
        else if (illuminationLEDstatus == 30) illuminationBrightness(255);   // turn led on full
        else illuminationBrightness(0);                                      // turn led off
        if (serialDebug) Serial.println("Button 3 pressed");
      }

    // if exposure was adjusted - cameraImageExposure
        if (server.hasArg("exp")) {
          String Tvalue = server.arg("exp");   // read value
          if (Tvalue != NULL) {
            int val = Tvalue.toInt();
            if (val >= 0 && val <= 1200 && val != cameraImageExposure) { 
              if (serialDebug) Serial.printf("Exposure changed to %d\n", val);
              cameraImageExposure = val;
              cameraImageSettings();           // Apply camera image settings
            }
          }
        }

     // if image gain was adjusted - cameraImageGain
        if (server.hasArg("gain")) {
          String Tvalue = server.arg("gain");   // read value
            if (Tvalue != NULL) {
              int val = Tvalue.toInt();
              if (val >= 0 && val <= 31 && val != cameraImageGain) { 
                if (serialDebug) Serial.printf("Gain changed to %d\n", val);
                cameraImageGain = val;
                cameraImageSettings();          // Apply camera image settings
              }
            }
         }


   // html header
    client.write("<!DOCTYPE html> <html lang='en'> <head> <title>root</title> </head> <body>\n");         // basic html header
    client.write("<FORM action='/' method='post'>\n");       // used by the buttons in the html (action = the web page to send it to


  // --------------------------------------------------------------------


  // html main body
  //                    Info on the arduino ethernet library:  https://www.arduino.cc/en/Reference/Ethernet 
  //                                            Info in HTML:  https://www.w3schools.com/html/
  //     Info on Javascript (can be inserted in to the HTML):  https://www.w3schools.com/js/default.asp
  //                               Verify your HTML is valid:  https://validator.w3.org/
  
  
    client.write("<h1>Hello from ESP32Cam</h1>\n");

    // sd card details
      if (sdcardPresent) client.printf("<p>SD Card detected - %d images stored</p>\n", imageCounter);       
      else client.write("<p>No SD Card detected</p>\n");

    // io pin details
      if (digitalRead(iopinA) == LOW) client.write("<p>Output Pin 13 is Low</p>\n");
      else client.write("<p>Output Pin 13 is High</p>\n");
      
      if (digitalRead(iopinB) == LOW) client.write("<p>Output Pin 12 is Low</p>\n");
      else client.write("<p>Output Pin 12 is High</p>\n");
      
      if (digitalRead(iopinC) == LOW) client.write("<p>Input Pin 16 is Low</p>\n");
      else client.write("<p>Input Pin 16 is High</p>\n");

    // illumination led brightness
      client.printf("<p>Illumination led set to %d</p>\n", illuminationLEDstatus);

    // Current real time
      client.print("<p>Current time: " + localTime() + "</p>\n");

//    // touch input on the two gpio pins 
//      client.printf("<p>Touch on pin 12: %d </p>\n", touchRead(T5) );
//      client.printf("<p>Touch on pin 13: %d </p>\n", touchRead(T4) );

    // Control bottons 
      client.write("<input style='height: 35px;' name='button1' value='Toggle pin 13' type='submit'> \n");
      client.write("<input style='height: 35px;' name='button2' value='Toggle pin 12' type='submit'> \n");
      client.write("<input style='height: 35px;' name='button3' value='Toggle Flash' type='submit'><br> \n");

    // Image setting controls
      client.write("<br>CAMERA SETTINGS: \n");
      client.printf("Exposure: <input type='number' style='width: 50px' name='exp' min='0' max='1200' value='%d'>  \n", cameraImageExposure);
      client.printf("Gain: <input type='number' style='width: 50px' name='gain' min='0' max='30' value='%d'>\n", cameraImageGain); 
      client.write(" - Set both to zero for auto adjust<br>\n");      

    // links to the other pages available
      client.write("<br>LINKS: \n");
      client.write("<a href='/photo'>Capture an image</a> - \n"); 
      client.write("<a href='/img'>View stored images</a> - \n");      
      client.write("<a href='/rgb'>Access Image as RGB data</a> - \n");   
      client.write("<a href='/stream'>Live stream</a><br>\n");       
      
      
  
  // --------------------------------------------------------------------

    
  // end html
    client.write("</form></body></html>\n");
    delay(3);
    client.stop();

}  // handleRoot


// ******************************************************************************************************************


// ----------------------------------------------------------------
//    -photo save to sd card/spiffs    i.e. http://x.x.x.x/photo
// ----------------------------------------------------------------
// web page to capture an image from camera and save to spiffs or sd card

void handlePhoto() {

  WiFiClient client = server.client();                                                        // open link with client

  // log page request including clients IP address
    if (serialDebug) {
      IPAddress cip = client.remoteIP();
      Serial.printf("Photo requested from: %d.%d.%d.%d \n", cip[0], cip[1], cip[2], cip[3]);
    }

  // save an image to sd card or spiffs
    byte sRes = storeImage();              // save an image to sd card or spiffs (store sucess or failed flag - 0=fail, 1=spiffs only, 2=spiffs and sd card)
    
  // html header
    client.write("<!DOCTYPE html> <html lang='en'> <head> <title>photo</title> </head> <body>\n");         // basic html header

  // html body
    if (sRes == 2) {
        client.printf("<p>Image saved to sd card as image number %d </p>\n", imageCounter);
        client.write("<a href='/img'>View Image</a>\n");                // link to the image   
    } else if (sRes == 1) {
        client.write("<p>Image saved in Spiffs</p>\n");
        client.write("<a href='/img'>View Image</a>\n");                // link to the image   
    } else {
        client.write("<p>Error: Failed to save image to sd card</p>\n");     
    }
     
  // end html
    client.write("</body></html>\n");
    delay(3);
    client.stop();

}  // handlePhoto



// ----------------------------------------------------------------
// -display image stored on sd card or SPIFFS   i.e. http://x.x.x.x/img?img=x
// ----------------------------------------------------------------
// Display a previously stored image, default image = most recent
// returns 1 if image displayed ok

bool handleImg() {

    WiFiClient client = server.client();                 // open link with client
    bool pRes = 0; 

  // log page request including clients IP address
    if (serialDebug) {
      IPAddress cip = client.remoteIP();
      Serial.printf("Image display requested from: %d.%d.%d.%d \n", cip[0], cip[1], cip[2], cip[3]);
      if (imageCounter == 0) Serial.println("Error: no images to display");
    }
    
    int imgToShow = imageCounter;                        // default to showing most recent file

    // get image number from url parameter 
      if (server.hasArg("img")) {
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
        WiFiClient client = server.client();                       // open link with client
        client.write("<!DOCTYPE html> <html> <body>\n");
        client.write("<p>Error: Image not found</p?\n");
        delay(3);
        client.stop();
      }
    }

    // if stored in SPIFFS
    if (!sdcardPresent) {
      if (serialDebug) Serial.println("Displaying image from spiffs");   
      File f = SPIFFS.open(spiffsFilename, "r");                         // read file from spiffs
          if (!f) {
            if (serialDebug) Serial.println("Error reading " + spiffsFilename);
          }
          else {
              size_t sent = server.streamFile(f, "image/jpeg");     // send file to web page
              if (!sent) {
                if (serialDebug) Serial.println("Error sending " + spiffsFilename);
              } else {
                pRes = 1;                                           // flag sucess
              }
              f.close();               
          }      
    }
    return pRes;
}  // handleImg


// ******************************************************************************************************************


// ----------------------------------------------------------------
//                      -invalid web page requested
// ----------------------------------------------------------------
// Note: shows a different way to send the HTML reply

void handleNotFound() {

  String tReply;

  // log page request
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


// ******************************************************************************************************************


// ----------------------------------------------------------------
//      -access image data as RGB - i.e. http://x.x.x.x/rgb
// ----------------------------------------------------------------
//Demonstration on how to access raw RGB data from the camera
// Notes:
//        Set sendRGBfile to 1 in the settings at top of sketch to just send the rgb data as a file
//          If this is all that is sent then it will download to the client as a raw RGB file which you can then view using this
//          Processing sketch: https://github.com/alanesq/esp32cam-demo/blob/master/Misc/displayRGB.pde
//        You may want to disable auto white balance when experimenting with RGB otherwise the camera is always trying to adjust the 
//          image colours to mainly white.   (disable in the 'cameraImageSettings' procedure).
//        It will fail on the highest resolution (1600x1200) as it requires more than the 4mb of available psram to store the data (1600x1200x3 bytes)
// - I discovered how to read the RGB data from: https://github.com/Makerfabs/Project_Touch-Screen-Camera/blob/master/Camera_v2/Camera_v2.ino

void readRGBImage() {

  uint32_t tTimer;                                                                                           // used for timing operations
  WiFiClient client = server.client();                                                                       // open link with client

  // log page request including clients IP address
    if (serialDebug) {
      IPAddress cip = client.remoteIP();
      Serial.printf("RGB requested from: %d.%d.%d.%d \n", cip[0], cip[1], cip[2], cip[3]);
    }

  // html header
    //client.write("<!DOCTYPE html> <html lang='en'> <head> <title>photo</title> </head> <body>\n");          // basic html header
    
  MessageRGB(client,"LIVE IMAGE AS RGB DATA");                                                              // 'MessageRGB' sends the String to both serial port and web page
  
  //   ****** the main code for converting an image to RGB data *****
    
    // capture a live image from camera (as a jpg)
      camera_fb_t * fb = NULL;
      tTimer = millis();                                                                                    // store time that image capture started
      fb = esp_camera_fb_get();  
      MessageRGB(client, "Image capture took " + String(millis() - tTimer) + " milliseconds");              // report time it took to capture an image
      if (!fb) MessageRGB(client," -error capturing image from camera- ");  
      MessageRGB(client,"Image resolution=" + String(fb->width) + "x" + String(fb->height));                // display image resolution         

      
    // allocate memory to store the rgb data (in psram, 3 bytes per pixel)
      if (!psramFound()) MessageRGB(client," -error no psram found- ");
      MessageRGB(client,"Free psram before rgb data stored = " + String(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
      void *ptrVal = NULL;                                                                                 // create a pointer for memory location to store the data
      uint32_t ARRAY_LENGTH = fb->width * fb->height * 3;                                                  // calculate memory required to store the RGB data (i.e. number of pixels in the jpg image x 3)
      if (heap_caps_get_free_size( MALLOC_CAP_SPIRAM) <  ARRAY_LENGTH) MessageRGB(client," -error: not enough free psram to store the rgb data- ");
      ptrVal = heap_caps_malloc(ARRAY_LENGTH, MALLOC_CAP_SPIRAM);                                          // allocate memory space for the rgb data 
      uint8_t *rgb = (uint8_t *)ptrVal;                                                                    // create the 'rgb' array pointer to the allocated memory space
      MessageRGB(client,"Free psram after rgb data stored = " + String(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
  
    
    // convert the captured jpg image (fb) to rgb data (store in 'rgb' array)
      tTimer = millis();                                                                                   // store time that image conversion process started
      bool jpeg_converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb);     
      if (!jpeg_converted) MessageRGB(client," -error converting image to RGB- "); 
      MessageRGB(client, "Conversion from jpg to RGB took " + String(millis() - tTimer) + " milliseconds");// report how long the conversion took


    if (sendRGBfile) client.write(rgb, ARRAY_LENGTH);          // send the rgb data as a file 
      

  //   ****** examples of reading the resulting RGB data *****
      
    // display some of the resulting data
        uint32_t resultsToShow = 60;                                                                       // how much data to display
        MessageRGB(client,"R , G , B");
        for (uint32_t i = 0; i < resultsToShow-2; i+=3) {
          MessageRGB(client,String(rgb[i+2]) + "," + String(rgb[i+1]) + "," + String(rgb[i+0]));           // Red , Green , Blue
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
        MessageRGB(client,"Average Blue = " + String(aBlue));
        MessageRGB(client,"Average Green = " + String(aGreen));
        MessageRGB(client,"Average Red = " + String(aRed));   
        

  //   *******************************************************


  // end html
    if (!sendRGBfile) client.write("</body></html>\n");
    delay(3);
    client.stop();     

  // finished with the data so free up the space used in psram 
    esp_camera_fb_return(fb);
    heap_caps_free(ptrVal);

}  // readRGBImage



// send line of text to both serial port and web page
void MessageRGB(WiFiClient &client, String theText) {
      if (!sendRGBfile) client.print(theText + "<br>\n");      
      if (serialDebug || theText.indexOf('error') > 0) Serial.println(theText);      
}



// ******************************************************************************************************************

 
// ----------------------------------------------------------------
//                      -get time from ntp server
// ----------------------------------------------------------------

bool getNTPtime(int sec) {

  {
    uint32_t start = millis();
    do {
      time(&now);
      localtime_r(&now, &timeinfo);
      Serial.print(".");
      delay(10);
    } while (((millis() - start) <= (1000 * sec)) && (timeinfo.tm_year < (2016 - 1900)));
    if (timeinfo.tm_year <= (2016 - 1900)) return false;  // the NTP call was not successful
    Serial.print("now ");  Serial.println(now);
    char time_output[30];
    strftime(time_output, 30, "%a  %d-%m-%y %T", localtime(&now));
    Serial.println(time_output);
    Serial.println();
  }
  return true;
}


// ******************************************************************************************************************


// ----------------------------------------------------------------
//      -stream requested     i.e. http://x.x.x.x/stream
// ----------------------------------------------------------------
// Sends cam stream - thanks to Uwe Gerlach for the code showing me how to do this

void handleStream(){

  WiFiClient client = server.client();          // open link with client

  // log page request including clients IP address
    if (serialDebug) {
      IPAddress cip = client.remoteIP();
      Serial.printf("Video stream requested from: %d.%d.%d.%d \n", cip[0], cip[1], cip[2], cip[3]);
    }

  // HTML used in the web page
  const char HEADER[] = "HTTP/1.1 200 OK\r\n" \
                        "Access-Control-Allow-Origin: *\r\n" \
                        "Content-Type: multipart/x-mixed-replace; boundary=123456789000000000000987654321\r\n";
  const char BOUNDARY[] = "\r\n--123456789000000000000987654321\r\n";           // marks end of each image frame
  const char CTNTTYPE[] = "Content-Type: image/jpeg\r\nContent-Length: ";       // marks start of image data
  const int hdrLen = strlen(HEADER);         // length of the stored text, used when sending to web page
  const int bdrLen = strlen(BOUNDARY);
  const int cntLen = strlen(CTNTTYPE);

  // temp stores
    char buf[32];  
    int s;
    camera_fb_t * fb = NULL;

  // send html header 
    client.write(HEADER, hdrLen);
    client.write(BOUNDARY, bdrLen);

  // send live images until client disconnects
  while (true)
  {
    if (!client.connected()) break;
      fb = esp_camera_fb_get();                   // capture live image 
      s = fb->len;                                // store size of image (i.e. buffer length)
      client.write(CTNTTYPE, cntLen);             // send content type html (i.e. jpg image)
      sprintf( buf, "%d\r\n\r\n", s );            // format the image's size as html and put in to 'buf'
      client.write(buf, strlen(buf));             // send result (image size)
      client.write((char *)fb->buf, s);           // send the image data
      client.write(BOUNDARY, bdrLen);             // send html boundary      see https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Type
      esp_camera_fb_return(fb);                   // return image so memory can be released
  }
  
  if (serialDebug) Serial.println("Video stream stopped");
  delay(3);
  client.stop();

  
}  // handleStream


// ******************************************************************************************************************


// ----------------------------------------------------------------
//                        request a web page
// ----------------------------------------------------------------
//     parameters = ip address, page to request, port to use (usually 80), maximum chars to receive, ignore all in reply before this text 
//          e.g. String q = requestWebPage("192.168.1.166","/log",80,600,"");

String requestWebPage(String ip, String page, int port, int maxChars, String cuttoffText = ""){

  int maxWaitTime = 3000;                 // max time to wait for reply (ms)

  char received[maxChars + 1];            // temp store for incoming character data
  int received_counter = 0;               // number of characters which have been received

  if (!page.startsWith("/")) page = "/" + page;     // make sure page begins with "/" 

  if (serialDebug) {
    Serial.print("requesting web page: ");
    Serial.print(ip);
    Serial.println(page);
  }
     
    WiFiClient client;

    // Connect to the site 
      if (!client.connect(ip.c_str() , port)) {                                      
        if (serialDebug) Serial.println("Web client connection failed");   
        return "web client connection failed";
      } 
      if (serialDebug) Serial.println("Connected to host - sending request...");
    
    // send request - A basic request looks something like: "GET /index.html HTTP/1.1\r\nHost: 192.168.0.4:8085\r\n\r\n"
      client.print("GET " + page + " HTTP/1.1\r\n" +
                   "Host: " + ip + "\r\n" + 
                   "Connection: close\r\n\r\n");
  
      if (serialDebug) Serial.println("Request sent - waiting for reply...");
  
    // Wait for a response
      uint32_t ttimer = millis();
      while ( !client.available() && (uint32_t)(millis() - ttimer) < maxWaitTime ) {
        delay(10);
      }

    // read the response
      while ( client.available() && received_counter < maxChars ) {
        #if defined ESP8266
          delay(2);                          // it just reads 255s on esp8266 if this delay is not included
        #endif        
        received[received_counter] = char(client.read());     // read one character
        received_counter+=1;
      }
      received[received_counter] = '\0';     // end of string marker
            
    if (serialDebug) {
      Serial.println("--------received web page-----------");
      Serial.println(received);
      Serial.println("------------------------------------");
      Serial.flush();     // wait for serial data to finish sending
    }
    
    client.stop();    // close connection
    if (serialDebug) Serial.println("Connection closed");

    // if cuttoffText was supplied then only return the text following this 
      if (cuttoffText != "") {
        char* locus = strstr(received,cuttoffText.c_str());    // locus = pointer to the found text
        if (locus) {                                           // if text was found
          if (serialDebug) Serial.println("The text '" + cuttoffText + "' was found in reply");
          return locus;                                        // return the reply text following 'cuttoffText'
        } else if (serialDebug) Serial.println("The text '" + cuttoffText + "' WAS NOT found in reply");
      }
    
  return received;        // return the full reply text
}



// ******************************************************************************************************************


// ----------------------------------------------------------------
//       -test procedure    i.e. http://x.x.x.x/test
// ----------------------------------------------------------------

void handleTest() {

  WiFiClient client = server.client();                                                        // open link with client

  // log page request including clients IP address
    if (serialDebug) {
      IPAddress cip = client.remoteIP();
      Serial.printf("Test requested from: %d.%d.%d.%d \n", cip[0], cip[1], cip[2], cip[3]);
    }

  // html header
    client.write("<!DOCTYPE html> <html lang='en'> <head> <title>photo</title> </head> <body>\n");         // basic html header


  // html body
    client.print("<h1>Test Page</h1>\n");


  // -------------------------------------------------------------------



    
    
    
        // < YOUR TEST CODE GOES HERE> 




    
    


//  // demo useage of the mcp23017 io chip 
//    #if useMCP23017 == 1
//      while(1) {
//          mcp.digitalWrite(0, HIGH);
//          int q = mcp.digitalRead(8);
//          client.print("<p>HIGH, input =" + String(q) + "</p>");
//          delay(1000);
//          mcp.digitalWrite(0, LOW);
//          client.print("<p>LOW</p>");
//          delay(1000);
//      }
//    #endif



  // -------------------------------------------------------------------
  
     
  // end html
    client.write("</body></html>\n");
    delay(3);
    client.stop();

}  // handleTest


// ******************************************************************************************************************
// end
