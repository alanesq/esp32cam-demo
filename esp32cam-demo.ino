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
 * 
 * 
 *     Info on the esp32cam board:  https://randomnerdtutorials.com/esp32-cam-video-streaming-face-recognition-arduino-ide/
 *            
 *     To see a more advanced sketch along the same format as this one have a look at https://github.com/alanesq/CameraWifiMotion
 *        which includes email support, FTP, OTA updates, time from NTP servers and motion detection
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

  const String stitle = "ESP32Cam-demo";                 // title of this sketch
  const String sversion = "27Sep20";                     // Sketch version

  const bool debugInfo = 1;                              // show additional debug info. on serial port (1=enabled)

  // Camera related
  const bool flashRequired = 1;                          // If flash to be used when capturing image (1 = yes)
  const framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_VGA;    // Image resolution:   
                                                         //               default = "const framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_XGA"
                                                         //               160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA), 
                                                         //               320x240 (QVGA), 400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA), 
                                                         //               1024x768 (XGA), 1280x1024 (SXGA), 1600x1200 (UXGA)

  const int TimeBetweenStatus = 600;                     // speed of flashing system running ok status light (milliseconds)

  const int indicatorLED = 33;                           // onboard status LED pin (33)

  const int brightLED = 4;                               // onboard flash LED pin (4)

  const int iopinA = 13;                                 // general io pin
  const int iopinB = 12;                                 // general io pin (must be low at boot)


  
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

// camera type settings (CAMERA_MODEL_AI_THINKER)
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
  
  
  

// ******************************************************************************************************************


// ---------------------------------------------------------------
//    -SETUP     SETUP     SETUP     SETUP     SETUP     SETUP
// ---------------------------------------------------------------

void setup() {
  
  // Serial communication 
    Serial.begin(115200);
  
    Serial.println(("\n\n\n---------------------------------------"));
    Serial.println("Starting - " + stitle + " - " + sversion);
    Serial.println(("---------------------------------------"));

  // Turn-off the 'brownout detector'
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // Define indicator led
    pinMode(indicatorLED, OUTPUT);
    digitalWrite(indicatorLED,HIGH);

  // Connect to wifi
    digitalWrite(indicatorLED,LOW);               // small indicator led on
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();
    digitalWrite(indicatorLED,HIGH);               // small indicator led off

  // define web pages (call procedures when url is requested)
    server.on("/", handleRoot);               // root page
    server.on("/stream", handleStream);       // stream live video
    server.on("/photo", handlePhoto);         // save image to sd card
    server.on("/img", handleImg);             // show image from sd card
    server.onNotFound(handleNotFound);        // invalid url requested

  // set up camera
      Serial.print(("Initialising camera: "));
      if (setupCameraHardware()) Serial.println("OK");
      else {
        Serial.println("Error!");
        showError(2);       // critical error so stop and flash led
      }

  // Configure sd card
      if (!SD_MMC.begin("/sdcard", true)) {         // if loading sd card fails     
        // note: ("/sdcard", true) = 1 wire - see: https://www.reddit.com/r/esp32/comments/d71es9/a_breakdown_of_my_experience_trying_to_talk_to_an/
        Serial.println("No SD Card detected"); 
        sdcardPresent = 0;                    // flag no sd card available
      } else {
        uint8_t cardType = SD_MMC.cardType();
        if (cardType == CARD_NONE) {          // if invalid card found
            Serial.println("SD Card type detect failed"); 
            sdcardPresent = 0;                // flag no sd card available
        } else {
          // valid sd card detected
          uint16_t SDfreeSpace = (uint64_t)(SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024 * 1024);
          Serial.println("SD Card found, free space = " + String(SDfreeSpace) + "MB");  
          sdcardPresent = 1;                  // flag sd card available
        }
      }
      fs::FS &fs = SD_MMC;                    // sd card file system

  // discover number of image files stored /img of sd card and set image file counter accordingly
    if (sdcardPresent) {
      int tq=fs.mkdir("/img");              // create the "/img" folder on sd card if not already there
      if (!tq) Serial.println("Unable to create IMG folder on sd card");
      
      File root = fs.open("/img");
      while (true)
      {
        File entry =  root.openNextFile();
        if (! entry) break;
        imageCounter ++;    // increment image counter
        entry.close();
      }
      root.close();
      Serial.println("Image file count = " + String(imageCounter));
    }
   
  // define io pins 
    pinMode(indicatorLED, OUTPUT);        // re defined as sd card config can reset it
    digitalWrite(indicatorLED,HIGH);
    pinMode(brightLED, OUTPUT);
    digitalWrite(brightLED,LOW);
    pinMode(iopinA, OUTPUT);      // pin 13 - free io pin, can be input or output
    pinMode(iopinB, OUTPUT);      // pin 12 - free io pin, can be input or output (must be low at boot)

  if (!psramFound()) {
    Serial.println("Warning: No PSRam found so defaulting to image size 'CIF'");
    framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_CIF;
  }

  Serial.println("Started...");
  
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
 
    
  // flash status light to show sketch is running 
    if ((unsigned long)(millis() - lastStatus) >= TimeBetweenStatus) { 
      lastStatus = millis();                                               // reset timer
      digitalWrite(indicatorLED,!digitalRead(indicatorLED));               // flip indicator led status
    }
    
}  // loop



// ******************************************************************************************************************


// ----------------------------------------------------------------
//                       Misc small procedures
// ----------------------------------------------------------------


// flash led 'reps' number of times
void flashLED(int reps) {
  for(int x=0; x < reps; x++) {
    digitalWrite(indicatorLED,LOW);
    delay(1000);
    digitalWrite(indicatorLED,HIGH);
    delay(500);
  }
}



// critical error - stop sketch and continually flash error status 
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

void storeImage() {

  if (debugInfo) Serial.println("Storing image #" + String(imageCounter) + " to sd card");

  fs::FS &fs = SD_MMC;                              // sd card file system

  // capture live image from camera
  if (flashRequired) digitalWrite(brightLED,HIGH);  // turn flash on
  camera_fb_t *fb = esp_camera_fb_get();            // capture image frame from camera
  digitalWrite(brightLED,LOW);                      // turn flash off
  if (!fb) {
    Serial.println("Error: Camera capture failed");
    flashLED(3);
  }
  
  // save the image to sd card
    imageCounter ++;                                                              // increment image counter
    String SDfilename = "/img/" + String(imageCounter) + ".jpg";                  // build the image file name
    File file = fs.open(SDfilename, FILE_WRITE);                                  // create file on sd card
    if (!file) {
      Serial.println("Error: Failed to create file on sd-card: " + SDfilename);
      flashLED(4);
    } else {
      if (file.write(fb->buf, fb->len)) {                                         // File created ok so save image to it
        if (debugInfo) Serial.println("Image saved to sd card"); 
      } else {
        Serial.println("Error: failed to save image to sd card");
        flashLED(4);
      }
      file.close();                // close image file on sd card
    }
    esp_camera_fb_return(fb);        // return frame so memory can be released

} // storeImage



// ******************************************************************************************************************


// ----------------------------------------------------------------
//                       Configure the camera
// ----------------------------------------------------------------

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
    config.pixel_format = PIXFORMAT_JPEG;         // PIXFORMAT_ + YUV422, GRAYSCALE, RGB565, JPEG, RGB888?
    config.frame_size = FRAME_SIZE_IMAGE;         // Image sizes: 160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA), 320x240 (QVGA), 
                                                  //              400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA), 1024x768 (XGA), 1280x1024 (SXGA), 1600x1200 (UXGA)
    config.jpeg_quality = 5;                      // 0-63 lower number means higher quality
    config.fb_count = 1;                          // if more than one, i2s runs in continuous mode. Use only with JPEG
    
    esp_err_t camerr = esp_camera_init(&config);  // initialise the camera
    if (camerr != ESP_OK) Serial.printf("ERROR: Camera init failed with error 0x%x", camerr);

    // cameraImageSettings();                 // apply camera sensor settings
    
    return (camerr == ESP_OK);             // return boolean result of camera initilisation
}


// ******************************************************************************************************************


// ----------------------------------------------------------------
//       -root web page requested    i.e. http://x.x.x.x/
// ----------------------------------------------------------------

void handleRoot() {

  WiFiClient client = server.client();                                                        // open link with client
  String tstr;                                                                                // temp store for building line of html

  // log page request including clients IP address
      IPAddress cip = client.remoteIP();
      if (debugInfo) Serial.println("Root page requested from: " + String(cip[0]) +"." + String(cip[1]) + "." + String(cip[2]) + "." + String(cip[3]));

  // html header
    client.write("<!DOCTYPE html> <html> <body>\n");         // basic html header
    client.write("<FORM action='/' method='post'>\n");       // used by the buttons in the html (action = the web page to send it to)

  // if button1 was pressed 
  //        Note:  if using an input box etc. you would read the value with the command:    String Tvalue = server.arg("demobutton1"); 
    if (server.hasArg("button1")) {
      digitalWrite(iopinA,!digitalRead(iopinA));         // toggle output pin
      if (debugInfo) Serial.println("Button 1 pressed");
    }

  // if button2 was pressed 
    if (server.hasArg("button2")) {
      digitalWrite(iopinB,!digitalRead(iopinB));         // toggle output pin
      if (debugInfo) Serial.println("Button 2 pressed");
    }

  // if button3 was pressed 
    if (server.hasArg("button3")) {
      digitalWrite(brightLED,!digitalRead(brightLED));   // toggle flash LED
      if (debugInfo) Serial.println("Button 3 pressed");
    }
    

  // --------------------------------------------------------------------


  // html main body
  //     Info on the arduino ethernet library:  https://www.arduino.cc/en/Reference/Ethernet 
  //     Info in HTML:  https://www.w3schools.com/html/
  //     Info on Javascript (can be inserted in to the HTML):  https://www.w3schools.com/js/default.asp
  
  
    client.write("<h1>Hello from ESP32Cam</h1>\n");
    
    if (sdcardPresent) client.write("<p>SD Card detected</p>\n");
    else client.write("<p>No SD Card detected</p>\n");

    if (digitalRead(iopinA) == LOW) client.write("<p>Pin 13 is Low</p>\n");
    else client.write("<p>Pin 13 is High</p>\n");
    
    if (digitalRead(iopinB) == LOW) client.write("<p>Pin 12 is Low</p>\n");
    else client.write("<p>Pin 12 is High</p>\n");

    // Control bottons 
      client.write("<input style='height: 35px;' name='button1' value='Toggle pin 13' type='submit'> \n");
      client.write("<input style='height: 35px;' name='button2' value='Toggle pin 12' type='submit'> \n");
      client.write("<input style='height: 35px;' name='button3' value='Toggle Flash' type='submit'> \n");

      
  
  // --------------------------------------------------------------------

    
  // end html
    client.write("</body></htlm>\n");
    delay(3);
    client.stop();

}  // handleRoot


// ******************************************************************************************************************


// ----------------------------------------------------------------
//       -photo save to sd card    i.e. http://x.x.x.x/photo
// ----------------------------------------------------------------

void handlePhoto() {

  WiFiClient client = server.client();                                                        // open link with client
  String tstr;                                                                                // temp store for building line of html

  // log page request including clients IP address
      IPAddress cip = client.remoteIP();
      if (debugInfo) Serial.println("Photo to sd card requested from: " + String(cip[0]) +"." + String(cip[1]) + "." + String(cip[2]) + "." + String(cip[3]));

  // save an image to sd card
    storeImage();              // save an image to sd card

  // html header
    client.write("<!DOCTYPE html> <html> <body>\n");

  // html body
    // note: if the line of html is not just plain text it has to be first put in to 'tstr' then sent in this way
    //       I find it easier to use strings then convert but this is probably frowned upon ;-)
    tstr = "<p>Image saved to sd card as image number " + String(imageCounter) + "</p>\n";
    client.write(tstr.c_str());           
    
  // end html
    client.write("</body></htlm>\n");
    delay(3);
    client.stop();

}  // handlePhoto






// ----------------------------------------------------------------
//    -show image from sd card    i.e. http://x.x.x.x/img?img=x
// ----------------------------------------------------------------
// default image = most recent

void handleImg() {

    WiFiClient client = server.client();                 // open link with client
    int imgToShow = imageCounter;                        // default to showing most recent file

    // get image number from url parameter 
      if (server.hasArg("img")) {
        String Tvalue = server.arg("img");               // read value
        imgToShow = Tvalue.toInt();                      // convert string to int
        if (imgToShow < 1 || imgToShow > imageCounter) imgToShow = imageCounter;    // validate image number
      }

    if (debugInfo) Serial.println("Displaying image #" + String(imgToShow) + " from sd card");   
 
    String tFileName = "/img/" + String(imgToShow) + ".jpg";
    fs::FS &fs = SD_MMC;                    // sd card file system
    File timg = fs.open(tFileName, "r");
    if (timg) {
        size_t sent = server.streamFile(timg, "image/jpeg"); 
        timg.close();
    } else {
      if (debugInfo) Serial.println("Error: image file not found");
    }
    
}  // handleImg


// ******************************************************************************************************************


// ----------------------------------------------------------------
//                      -invalid web page requested
// ----------------------------------------------------------------

void handleNotFound() {
  
  if (debugInfo) Serial.println("Invalid web page requested");      
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
      IPAddress cip = client.remoteIP();
      if (debugInfo) Serial.println("Video stream requested from: " + String(cip[0]) +"." + String(cip[1]) + "." + String(cip[2]) + "." + String(cip[3]));

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
      fb = esp_camera_fb_get();                   // capture live image frame
      s = fb->len;                                // store size of image (i.e. buffer length)
      client.write(CTNTTYPE, cntLen);             // send content type html (i.e. jpg image)
      sprintf( buf, "%d\r\n\r\n", s );            // format the image's size as html 
      client.write(buf, strlen(buf));             // send image size 
      client.write((char *)fb->buf, s);           // send the image data
      client.write(BOUNDARY, bdrLen);             // send html boundary      see https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Type
      esp_camera_fb_return(fb);                   // return frame so memory can be released
  }
  
  if (debugInfo) Serial.println("Video stream stopped");
  delay(3);
  client.stop();

  
}  // handleStream


// ******************************************************************************************************************
// end
