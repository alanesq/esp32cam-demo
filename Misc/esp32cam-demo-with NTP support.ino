 /*******************************************************************************************************************
 *            
 *                                 ESP32Cam development board demo sketch using Arduino IDE
 *                                    Github: https://github.com/alanesq/ESP32Cam-demo
 *     
 *     Starting point sketch for projects using the esp32cam development board with the following features
 *        web server with live video streaming
 *        sd card support (using 1bit mode to free some io pins)
 *        io pins available for use are 13 and 12 (12 must be low at boot)
 *        flash led is still available for use on pin 4 and does not flash when accessing sd card
 * 
 *     
 *     - created using the Arduino IDE with ESP32 module installed   (https://dl.espressif.com/dl/package_esp32_index.json)
 *       No additional libraries required
 * 
 *     ESP32 support for Arduino IDE: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
 * 
 *     Info on the esp32cam board:  https://randomnerdtutorials.com/esp32-cam-video-streaming-face-recognition-arduino-ide/
 *            
 *     To see a more advanced sketch along the same format as this one have a look at https://github.com/alanesq/CameraWifiMotion
 *        which includes email support, FTP, OTA updates, time from NTP servers and motion detection
 * 
 *     Example of how to use the image data:   https://forum.arduino.cc/?topic=708266#msg4760255
 * 
 *        
 * 
 *     esp32cam-demo is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the 
 *        implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * 
 *******************************************************************************************************************/


#include "esp_camera.h"       // https://github.com/espressif/esp32-camera
#include <WiFi.h>
#include <WebServer.h>


// ---------------------------------------------------------------
//                          -SETTINGS
// ---------------------------------------------------------------

  // Wifi settings (enter your wifi network details)

  const char* ssid     = "<your wifi network name here>";
  const char* password = "<your wifi password here>";

  const char* stitle = "ESP32Cam-demo-NTP";                  // title of this sketch
  const char* sversion = "20Oct20";                      // Sketch version

  const bool debugInfo = 1;                              // show additional debug info. on serial port (1=enabled)

  // Camera related
  const bool flashRequired = 1;                          // If flash to be used when capturing image (1 = yes)
  const framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_VGA;    // Image resolution:   
                                                         //               default = "const framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_XGA"
                                                         //               160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA), 
                                                         //               320x240 (QVGA), 400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA), 
                                                         //               1024x768 (XGA), 1280x1024 (SXGA), 1600x1200 (UXGA)
  int cameraImageExposure = 0;                           // Camera exposure (0 - 1200), if gain and exposure set to zero then auto adjust is enabled
  int cameraImageGain = 0;                               // Image gain (0 - 30)

  const int TimeBetweenStatus = 600;                     // speed of flashing system running ok status light (milliseconds)

  const int indicatorLED = 33;                           // onboard status LED pin (33)

  const int brightLED = 4;                               // onboard flash LED pin (4)

  const int iopinA = 13;                                 // general io pin
  const int iopinB = 12;                                 // general io pin (must be low at boot)
  
  const int serialSpeed = 115200;                        // Serial data speed to use


  
// ******************************************************************************************************************


WebServer server(80);                       // serve web pages on port 80

#include "soc/soc.h"                        // Used to disable brownout detection 
#include "soc/rtc_cntl_reg.h"      

#include "SD_MMC.h"                         // sd card - see https://randomnerdtutorials.com/esp32-cam-take-photo-save-microsd-card/
#include <SPI.h>                       
#include <FS.h>                             // gives file access 
#define SD_CS 5                             // sd chip select pin = 5
  
// Define global variables:
  uint32_t lastStatus = millis();           // last time status light changed status (to flash all ok led)
  uint32_t lastCamera = millis();           // timer for periodic image capture
  bool sdcardPresent;                       // flag if an sd card is detected
  int imageCounter;                         // image file name on sd card counter

// camera settings (for the standard - OV2640 - CAMERA_MODEL_AI_THINKER)
//     see: https://randomnerdtutorials.com/esp32-cam-camera-pin-gpios/
  #define CAMERA_MODEL_AI_THINKER
  #define PWDN_GPIO_NUM     32      // power to camera on/off
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
  
  
// Time from NTP server
//      from https://raw.githubusercontent.com/RalphBacon/No-Real-Time-Clock-RTC-required---use-an-NTP/master
  #include <TimeLib.h>
  #include <WiFiUdp.h>                         // UDP library which is how we communicate with Time Server
  const uint16_t localPort = 8888;              // Just an open port we can use for the UDP packets coming back in
  const char timeServer[] = "uk.pool.ntp.org"; 
  const uint16_t NTP_PACKET_SIZE = 48;          // NTP time stamp is in the first 48 bytes of the message
  byte packetBuffer[NTP_PACKET_SIZE];          // buffer to hold incoming and outgoing packets
  WiFiUDP NTPUdp;                              // A UDP instance to let us send and receive packets over UDP
  const uint16_t timeZone = 0;                  // timezone (0=GMT)
  const String DoW[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  // How often to resync the time (under normal and error conditions)
    const uint16_t _resyncSeconds = 7200;       // 7200 = 2 hours
    const uint16_t _resyncErrorSeconds = 60;    // 60 = 1 min
  bool NTPok = 0;                              // Flag if NTP is curently connecting ok
  
  
// ******************************************************************************************************************


// ---------------------------------------------------------------
//    -SETUP     SETUP     SETUP     SETUP     SETUP     SETUP
// ---------------------------------------------------------------

void setup() {
  
  Serial.begin(serialSpeed);                     // Start serial communication 
  
  Serial.println("\n\n\n");                      // line feeds
  Serial.println("-----------------------------------");
  Serial.printf("Starting - %s - %s \n", stitle, sversion);  
  Serial.println("-----------------------------------");

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);     // Turn-off the 'brownout detector'

  // Define small indicator led
    pinMode(indicatorLED, OUTPUT);
    digitalWrite(indicatorLED,HIGH);

  // Connect to wifi
    digitalWrite(indicatorLED,LOW);               // small indicator led on
    Serial.print("\nConnecting to ");
    Serial.print(ssid);
    Serial.print("\n   ");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("\nWiFi connected, ");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();
    digitalWrite(indicatorLED,HIGH);              // small indicator led off

  // define the web pages (i.e. call these procedures when url is requested)
    server.on("/", handleRoot);                   // root page
    server.on("/stream", handleStream);           // stream live video
    server.on("/photo", handlePhoto);             // save image to sd card
    server.on("/img", handleImg);                 // show image from sd card
    server.onNotFound(handleNotFound);            // invalid url requested

  // set up camera
      Serial.print(("\nInitialising camera: "));
      if (setupCameraHardware()) Serial.println("OK");
      else {
        Serial.println("Error!");
        showError(2);                              // critical error so stop and flash led
      }

  // SD Card - if one is detected set 'sdcardPresent' High
      if (!SD_MMC.begin("/sdcard", true)) {        // if loading sd card fails     
        // note: ('/sdcard", true)' = 1bit mode - see: https://www.reddit.com/r/esp32/comments/d71es9/a_breakdown_of_my_experience_trying_to_talk_to_an/
        Serial.println("No SD Card detected"); 
        sdcardPresent = 0;                        // flag no sd card available
      } else {
        uint8_t cardType = SD_MMC.cardType();
        if (cardType == CARD_NONE) {              // if invalid card found
            Serial.println("SD Card type detect failed"); 
            sdcardPresent = 0;                    // flag no sd card available
        } else {
          // valid sd card detected
          uint16_t SDfreeSpace = (uint64_t)(SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024 * 1024);
          Serial.printf("SD Card found, free space = %dMB \n", SDfreeSpace);  
          sdcardPresent = 1;                      // flag sd card available
        }
      }
      fs::FS &fs = SD_MMC;                        // sd card file system

  // discover the number of image files stored in '/img' folder of the sd card and set image file counter accordingly
    imageCounter = 0;
    if (sdcardPresent) {
      int tq=fs.mkdir("/img");                    // create the '/img' folder on sd card (in case it is not already there)
      if (!tq) Serial.println("Unable to create IMG folder on sd card");

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
        Serial.printf("Image file count = %d",imageCounter);
    }
   
  // define io pins 
    pinMode(indicatorLED, OUTPUT);            // defined again as sd card config can reset it
    digitalWrite(indicatorLED,HIGH);          // led off = High
    pinMode(brightLED, OUTPUT);               // flash LED
    digitalWrite(brightLED,LOW);              // led off = Low
    pinMode(iopinA, OUTPUT);                  // pin 13 - free io pin, can be input or output
    pinMode(iopinB, OUTPUT);                  // pin 12 - free io pin, can be input or output (must be low at boot)

  if (!psramFound()) {
    Serial.println("Warning: No PSRam found so defaulting to image size 'CIF'");
    framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_CIF;
  }
  
  // start NTP
    NTPUdp.begin(localPort);                  // What port will the UDP/NTP packet respond on?
    setSyncProvider(getNTPTime);              // What is the function that gets the time (in ms since 01/01/1900)?
    setSyncInterval(_resyncErrorSeconds);     // How often should we synchronise the time on this machine (in seconds) 

  Serial.println("\n\nStarted...");

  cameraImageSettings();                      // Apply camera image settings
  
}  // setup



// ******************************************************************************************************************


// ----------------------------------------------------------------
//   -LOOP     LOOP     LOOP     LOOP     LOOP     LOOP     LOOP
// ----------------------------------------------------------------


void loop() {

  server.handleClient();          // handle any incoming web page requests





  
  //    <<< your code here >>>





//  //  Capture an image and save to sd card every 5 seconds 
//      if ((unsigned long)(millis() - lastCamera) >= 5000) { 
//        lastCamera = millis();     // reset timer
//        storeImage();              // save an image to sd card
//      }
 
    
  // flash status LED to show sketch is running 
    if ((unsigned long)(millis() - lastStatus) >= TimeBetweenStatus) { 
      lastStatus = millis();                                               // reset timer
      digitalWrite(indicatorLED,!digitalRead(indicatorLED));               // flip indicator led status
      time_t t=now();                                                      // read current time to ensure NTP auto refresh keeps triggering (otherwise only triggers when time is required causing a delay in response)
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
                                                  //              400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA), 1024x768 (XGA), 1280x1024 (SXGA), 1600x1200 (UXGA)
    config.jpeg_quality = 5;                      // 0-63 lower number means higher quality
    config.fb_count = 1;                          // if more than one, i2s runs in continuous mode. Use only with JPEG
    
    esp_err_t camerr = esp_camera_init(&config);  // initialise the camera
    if (camerr != ESP_OK) Serial.printf("ERROR: Camera init failed with error 0x%x", camerr);

    
    return (camerr == ESP_OK);                    // return boolean result of camera initialisation
}



// ******************************************************************************************************************


// ----------------------------------------------------------------
//                        Misc small procedures
// ----------------------------------------------------------------



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
//           Capture image from camera and save to sd card
// ----------------------------------------------------------------
// returns TRUE if sucessful

bool storeImage() {

  if (sdcardPresent) {
    if (debugInfo) Serial.printf("Storing image #%d to sd card \n", imageCounter);
  } else {
    if (debugInfo) Serial.println("Storing image requested but there is no sd card");
    return 0;           // no sd card available so exit procedure
  }

  fs::FS &fs = SD_MMC;                              // sd card file system
  bool tResult = 0;                                 // result flag

  // capture live image from camera
  if (flashRequired) digitalWrite(brightLED,HIGH);  // turn flash on
  camera_fb_t *fb = esp_camera_fb_get();            // capture image frame from camera
  digitalWrite(brightLED,LOW);                      // turn flash off
  if (!fb) {
    Serial.println("Error: Camera capture failed");
    flashLED(3);
  }
  
  // save the image to sd card
    String SDfilename = "/img/" + String(imageCounter + 1) + ".jpg";              // build the image file name
    File file = fs.open(SDfilename, FILE_WRITE);                                  // create file on sd card
    if (!file) {
      Serial.println("Error: Failed to create file on sd-card: " + SDfilename);
      flashLED(4);
    } else {
      if (file.write(fb->buf, fb->len)) {                                         // File created ok so save image to it
        if (debugInfo) Serial.println("Image saved to sd card"); 
        tResult = 1;                                                              // set sucess flag
        imageCounter ++;                                                          // increment image counter
      } else {
        Serial.println("Error: failed to save image to sd card");
        flashLED(4);
      }
      file.close();                // close image file on sd card
    }
    esp_camera_fb_return(fb);        // return frame so memory can be released
    
    return tResult;                  // return image save sucess flag

} // storeImage



// ******************************************************************************************************************


// ----------------------------------------------------------------
//       -root web page requested    i.e. http://x.x.x.x/
// ----------------------------------------------------------------

void handleRoot() {

  WiFiClient client = server.client();                       // open link with client

  // log the page request including clients IP address
    if (debugInfo) {
      IPAddress cip = client.remoteIP();
      Serial.printf("Root page requested from: %d.%d.%d.%d \n", cip[0], cip[1], cip[2], cip[3]);
    }


  // Action any button presses or settings entered on web page

    // if button1 was pressed (toggle io pin A)
    //        Note:  if using an input box etc. you would read the value with the command:    String Bvalue = server.arg("demobutton1"); 
      if (server.hasArg("button1")) {
        digitalWrite(iopinA,!digitalRead(iopinA));             // toggle output pin on/off
        if (debugInfo) Serial.println("Button 1 pressed");
      }
  
    // if button2 was pressed (toggle io pin B)
      if (server.hasArg("button2")) {
        digitalWrite(iopinB,!digitalRead(iopinB));             // toggle output pin on/off
        if (debugInfo) Serial.println("Button 2 pressed");
      }
  
    // if button3 was pressed (toggle flash LED)
      if (server.hasArg("button3")) {
        digitalWrite(brightLED,!digitalRead(brightLED));       // toggle flash LED on/off
        if (debugInfo) Serial.println("Button 3 pressed");
      }

    // if exposure was adjusted - cameraImageExposure
        if (server.hasArg("exp")) {
          String Tvalue = server.arg("exp");   // read value
          if (Tvalue != NULL) {
            int val = Tvalue.toInt();
            if (val >= 0 && val <= 1200 && val != cameraImageExposure) { 
              if (debugInfo) Serial.printf("Exposure changed to %d\n", val);
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
                if (debugInfo) Serial.printf("Gain changed to %d\n", val);
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
      if (digitalRead(iopinA) == LOW) client.write("<p>Pin 13 is Low</p>\n");
      else client.write("<p>Pin 13 is High</p>\n");
      
      if (digitalRead(iopinB) == LOW) client.write("<p>Pin 12 is Low</p>\n");
      else client.write("<p>Pin 12 is High</p>\n");
      
    // Read time from NTP
      client.print("<p>Time: " + currentTime() + "</p>");

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
      client.write("<a href='/stream'>Live stream</a><br>\n");       
      
      
  
  // --------------------------------------------------------------------

    
  // end html
    client.write("</form></body></html>\n");
    delay(3);
    client.stop();

}  // handleRoot


// ******************************************************************************************************************


// ----------------------------------------------------------------
//       -photo save to sd card    i.e. http://x.x.x.x/photo
// ----------------------------------------------------------------

void handlePhoto() {

  WiFiClient client = server.client();                                                        // open link with client

  // log page request including clients IP address
    if (debugInfo) {
      IPAddress cip = client.remoteIP();
      Serial.printf("Photo requested from: %d.%d.%d.%d \n", cip[0], cip[1], cip[2], cip[3]);
    }

  // save an image to sd card
    bool sRes = storeImage();              // save an image to sd card (store sucess or failed flag)
    
  // html header
    client.write("<!DOCTYPE html> <html lang='en'> <head> <title>photo</title> </head> <body>\n");         // basic html header

  // html body
    if (sRes == 1) {
        client.printf("<p>Image saved to sd card as image number %d </p>\n", imageCounter);
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
//    -show image from sd card    i.e. http://x.x.x.x/img?img=x
// ----------------------------------------------------------------
// default image = most recent
// returns 1 if image displayed ok

bool handleImg() {

    WiFiClient client = server.client();                 // open link with client

  // log page request including clients IP address
    if (debugInfo) {
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

    if (debugInfo) Serial.printf("Displaying image #%d from sd card", imgToShow);   
 
    String tFileName = "/img/" + String(imgToShow) + ".jpg";
    fs::FS &fs = SD_MMC;                                 // sd card file system
    File timg = fs.open(tFileName, "r");
    if (timg) {
        size_t sent = server.streamFile(timg, "image/jpeg");     // send the image
        timg.close();
    } else {
      if (debugInfo) Serial.println("Error: image file not found");
      WiFiClient client = server.client();                       // open link with client
      client.write("<!DOCTYPE html> <html> <body>\n");
      client.write("<p>Error: Image not found</p?\n");
      delay(3);
      client.stop();
    }
    
}  // handleImg


// ******************************************************************************************************************


// ----------------------------------------------------------------
//                      -invalid web page requested
// ----------------------------------------------------------------

void handleNotFound() {

  // log page request
    if (debugInfo) {
      Serial.print("Invalid page requested");
    }
  
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
  message = "";      // clear variable
  
}  // handleNotFound


// ******************************************************************************************************************


// ----------------------------------------------------------------
//      -stream requested     i.e. http://x.x.x.x/stream
// ----------------------------------------------------------------
// Sends cam stream - thanks to Uwe Gerlach for the code showing how to do this

void handleStream(){

  WiFiClient client = server.client();          // open link with client

  // log page request including clients IP address
    if (debugInfo) {
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
  
  if (debugInfo) Serial.println("Video stream stopped");
  delay(3);
  client.stop();

  
}  // handleStream


// ******************************************************************************************************************

// ----------------------------------------------------------------
//                      -Change camera settings
// ----------------------------------------------------------------
// Returns TRUE is successful

bool cameraImageSettings() { 
   
    sensor_t *s = esp_camera_sensor_get();       
    if (s == NULL) {
      Serial.println("Error: problem getting camera sensor settings");
      return 0;
    } 

    if (cameraImageExposure == 0 && cameraImageGain == 0) {              
      // enable auto adjust
        s->set_gain_ctrl(s, 1);                       // auto gain on 
        s->set_exposure_ctrl(s, 1);                   // auto exposure on 
    } else {
      // Apply manual settings
        s->set_gain_ctrl(s, 0);                       // auto gain off 
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







// --------------------------------------------------------------------------------------------------------------
//                                                   NTP Real Time
// --------------------------------------------------------------------------------------------------------------


// ----------------------------------------------------------------
//               -Return current time and date as string
// ----------------------------------------------------------------
// supplies time in the format:   '23-04-2020_09-23-10_Mon'

String currentTime() {

   time_t t=now();     // get current time 
   t+=timeZone;        // adjust for timezone

   if (IsBST()) t+=3600;     // add one hour if it is Summer Time

   String ttime = formatDateNumber(day(t));
   ttime += "-";
   ttime += formatDateNumber(month(t));
   ttime += "-";
   ttime += formatDateNumber(year(t));
   ttime += "_";
   ttime += formatDateNumber(hour(t));
   ttime += "-";
   ttime += formatDateNumber(minute(t));
   ttime += "-";
   ttime += formatDateNumber(second(t));
   ttime += "_";
   ttime += DoW[weekday(t)-1];

   return ttime;
}


// convert number to String and add leading zero if required
String formatDateNumber(int input) {
  String tval = "";
  if (input < 10) tval = "0";    // add leading zero if required   
  tval += String(input);
  return tval;
}



//-----------------------------------------------------------------------------
//                           -British Summer Time check
//-----------------------------------------------------------------------------

// returns true if it is British Summer time
//         code from https://my-small-projects.blogspot.com/2015/05/arduino-checking-for-british-summer-time.html

boolean IsBST()
{
    int imonth = month();
    int iday = day();
    int hr = hour();
    
    //January, february, and november are out.
    if (imonth < 3 || imonth > 10) { return false; }
    //April to September are in
    if (imonth > 3 && imonth < 10) { return true; }

    // find last sun in mar and oct - quickest way I've found to do it
    // last sunday of march
    int lastMarSunday =  (31 - (5* year() /4 + 4) % 7);
    //last sunday of october
    int lastOctSunday = (31 - (5 * year() /4 + 1) % 7);
        
    //In march, we are BST if is the last sunday in the month
    if (imonth == 3) { 
      
      if( iday > lastMarSunday)
        return true;
      if( iday < lastMarSunday)
        return false;
      
      if (hr < 1)
        return false;
              
      return true; 
  
    }
    //In October we must be before the last sunday to be bst.
    //That means the previous sunday must be before the 1st.
    if (imonth == 10) { 

      if( iday < lastOctSunday)
        return true;
      if( iday > lastOctSunday)
        return false;  
      
      if (hr >= 1)
        return false;
        
      return true;  
    }

}



//-----------------------------------------------------------------------------
//        send an NTP request to the time server at the given address
//-----------------------------------------------------------------------------

void sendNTPpacket(const char* address) {
  
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;            // Stratum, or type of clock
  packetBuffer[2] = 6;            // Polling Interval
  packetBuffer[3] = 0xEC;         // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  // all NTP fields have been given values, now you can send a packet requesting a timestamp:
  // Note that Udp.begin will request automatic translation (via a DNS server) from a
  // name (eg pool.ntp.org) to an IP address. Never use a specific IP address yourself,
  // let the DNS give back a random server IP address
  NTPUdp.beginPacket(address, 123); //NTP requests are to port 123

  // Get the data back
  NTPUdp.write(packetBuffer, NTP_PACKET_SIZE);

  // All done, the underlying buffer is now updated
  NTPUdp.endPacket();
  
}



//-----------------------------------------------------------------------------
//                contact the NTP pool and retrieve the time
//-----------------------------------------------------------------------------
//
// code from https://github.com/RalphBacon/No-Real-Time-Clock-RTC-required---use-an-NTP

time_t getNTPTime() {

  // Send a UDP packet to the NTP pool address
  Serial.print("\nSending NTP packet to ");
  Serial.println(timeServer);
  sendNTPpacket(timeServer);

  // Wait to see if a reply is available - timeout after X seconds. At least
  // this way we exit the 'delay' as soon as we have a UDP packet to process
  #define UDPtimeoutSecs 3
  int timeOutCnt = 0;
  while (NTPUdp.parsePacket() == 0 && ++timeOutCnt < (UDPtimeoutSecs * 10)){
    delay(100);
    // yield();
  }

  // Is there UDP data present to be processed? Sneak a peek!
  if (NTPUdp.peek() != -1) {
    // We've received a packet, read the data from it
    NTPUdp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    // The time-stamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);

    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900)
    unsigned long secsSince1900 = highWord << 16 | lowWord;     // shift highword 16 binary places to the left then combine with lowword
    Serial.print("Seconds since Jan 1 1900 = ");
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    //Serial.print("Unix time = ");

    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;     // UL denotes it is 'unsigned long' 

    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;

    // Reset the interval to get the time from NTP server in case we previously changed it
    setSyncInterval(_resyncSeconds);
    NTPok = 1;       // flag NTP is currently connecting ok

    return epoch;
  }

  // Failed to get an NTP/UDP response
    Serial.println("No response received from NTP");
    setSyncInterval(_resyncErrorSeconds);       // try more frequently until a response is received
    NTPok = 0;                                  // flag NTP not currently connecting

    return 0;
  
}




// ******************************************************************************************************************
// end
