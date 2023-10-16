/**************************************************************************************************
 *
 *      Over The Air updates (OTA) - 02Aug23
 *      MODIFIED FOR USE WITH ESP32CamDemo  (no log, different header)
 *
 *      part of the BasicWebserver sketch - https://github.com/alanesq/BasicWebserver
 *
 *      If using an esp32cam module In Arduino IDE Select "ESP32 dev module" not "ESP32-cam" with PSRAM enabled
 *
 **************************************************************************************************

    Make sure partition with OTA enabled is selected

    To enable/disable OTA updates see setting at top of main sketch (#define ENABLE_OTA 1)

    Then access with    http://<esp ip address>/ota


 **************************************************************************************************/


#if defined ESP32
  #include <Update.h>
#endif


// forward declarations (i.e. details of all functions in this file)
  void otaSetup();
  void handleOTA();


// some useful html/css 
  const char colRed[] = "<span style='color:red;'>";        // red text
  const char colGreen[] = "<span style='color:green;'>";    // green text
  const char colBlue[] = "<span style='color:blue;'>";      // blue text
  const char colEnd[] = "</span>";                          // end coloured text
  const char htmlSpace[] = "&ensp;";                        // leave a space  (see 'HTML entity')


// ----------------------------------------------------------------
//     -enable OTA
// ----------------------------------------------------------------
// Enable OTA updates, called when correct password has been entered

void otaSetup() {

    OTAEnabled = 1;          // flag that OTA has been enabled

    // esp32 version (using webserver.h)
    #if defined ESP32
        server.on("/update", HTTP_POST, []() {
          server.sendHeader("Connection", "close");
          server.send(200, "text/plain", (Update.hasError()) ? "Update Failed!, rebooting..." : "Update complete, rebooting...");
          delay(2000);
          ESP.restart();
          delay(2000);
        }, []() {
          HTTPUpload& upload = server.upload();
          if (upload.status == UPLOAD_FILE_START) {
            if (serialDebug) Serial.setDebugOutput(true);
            if (serialDebug) Serial.printf("Update: %s\n", upload.filename.c_str());
            if (!Update.begin()) {        //start with max available size
              if (serialDebug) Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
              if (serialDebug) Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {      //true to set the size to the current progress
              if (serialDebug) Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
            } else {
              if (serialDebug) Update.printError(Serial);
            }
            if (serialDebug) Serial.setDebugOutput(false);
          } else {
            if (serialDebug) Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
          }
        });
    #endif

    // esp8266 version  (using ESP8266WebServer.h)
    #if defined ESP8266
        server.on("/update", HTTP_POST, []() {
          server.sendHeader("Connection", "close");
          server.send(200, "text/plain", (Update.hasError()) ? "Update Failed!, rebooting..." : "Update complete, rebooting...");
          delay(2000);
          ESP.restart();
          delay(2000);
        }, []() {
          HTTPUpload& upload = server.upload();
          if (upload.status == UPLOAD_FILE_START) {
            if (serialDebug) Serial.setDebugOutput(true);
            WiFiUDP::stopAll();
            if (serialDebug) Serial.printf("Update: %s\n", upload.filename.c_str());
            uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if (!Update.begin(maxSketchSpace)) { //start with max available size
              if (serialDebug) Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
              if (serialDebug) Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) { //true to set the size to the current progress
              if (serialDebug) Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
            } else {
              if (serialDebug) Update.printError(Serial);
            }
            if (serialDebug) Serial.setDebugOutput(false);
          }
          yield();
        });
    #endif

}


// ----------------------------------------------------------------
//     -OTA web page requested     i.e. http://x.x.x.x/ota
// ----------------------------------------------------------------
// Request OTA password or implement OTA update if already entered

void handleOTA(){

  WiFiClient client = server.client();          // open link with client

  // check if valid password supplied
    if (server.hasArg("pwd")) {
      if (server.arg("pwd") == OTAPassword) otaSetup();    // Enable over The Air updates (OTA)
    }


  // -----------------------------------------

  if (OTAEnabled == 0) {

    // OTA is not enabled so request password to enable it

      sendHeader(client, stitle);
            
      // This is the below javascript/html compacted to save flash memory via https://www.textfixer.com/html/compress-html-compression.php
      client.print (R"=====(<form name='loginForm'> <table width='20%' bgcolor='A09F9F' align='center'> <tr> <td colspan=2> <center><font size=4><b>Enter OTA password</b></font></center><br> </td> <br> </tr><tr> <td>Password:</td> <td><input type='Password' size=25 name='pwd'><br></td><br><br> </tr><tr> <td><input type='submit' onclick='check(this.form)' value='Login'></td> </tr> </table> </form> <script> function check(form) { window.open('/ota?pwd=' + form.pwd.value , '_self') } </script>)=====");
      /*
      client.print (R"=====(
         <form name='loginForm'>
            <table width='20%' bgcolor='A09F9F' align='center'>
                <tr>
                    <td colspan=2>
                        <center><font size=4><b>Enter OTA password</b></font></center><br>
                    </td><br>
                </tr><tr>
                    <td>Password:</td>
                    <td><input type='Password' size=25 name='pwd'><br></td><br><br>
                </tr><tr>
                    <td><input type='submit' onclick='check(this.form)' value='Login'></td>
                </tr>
            </table>
        </form>
        <script>
            function check(form)
            {
              window.open('/ota?pwd=' + form.pwd.value , '_self')
            }
        </script>
      )=====");
      */
      
      
      sendFooter(client);     // close web page

  }

  // -----------------------------------------

  
  if (OTAEnabled == 1) {                // if OTA is enabled implement it

      sendHeader(client, stitle);

      client.write("<br><H1>Update firmware</H1><br>\n");
      client.printf("Current version =  %s, %s \n\n", stitle, sversion);

      client.write("<form method='POST' action='/update' enctype='multipart/form-data'>\n");
      client.write("<input type='file' style='width: 300px' name='update'>\n");
      client.write("<br><br><input type='submit' value='Update'></form><br>\n");

      client.write("<br><br>Device will reboot when upload complete");
      client.printf("%s <br>To disable OTA restart device<br> %s \n", colRed, colEnd);

      sendFooter(client);     // close web page
  }

  // -----------------------------------------


  // close html page
    delay(3);
    client.stop();

}


// ---------------------------------------------- end ----------------------------------------------
