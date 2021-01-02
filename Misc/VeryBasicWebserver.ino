// ----------------------------------------------------------------
//
//
//       ESP32 / ESp8266  very basic web server demo - 02Jan21
//
//               Starting point to experiment with
//
//
// ----------------------------------------------------------------


// Wifi settings
  const char *SSID = "your_wifi_ssid";
  const char *PWD = "your_wifi_pwd";


bool serialDebug = 1;          // if to enable debugging info on serial port


// ----------------------------------------------------------------
  

//#include <arduino.h>         // required by platformio?

#if defined ESP32
    // esp32
        byte LEDpin = 2; 
        #include <WiFi.h>
        #include <WebServer.h>
        #include <HTTPClient.h>             // Web server running on port 80
        WebServer server(80);
#elif defined ESP8266
    //Esp8266
        byte LEDpin = D4; 
        #include <ESP8266WiFi.h>  
        #include <ESP8266WebServer.h>  
        #include "ESP8266HTTPClient.h"    
        ESP8266WebServer server(80);  
#else
      #error "This sketch only works with the ESP8266 or ESP32"
#endif


// ----------------------------------------------------------------

  
// this section runs once at startup 

  
void setup() {

  Serial.begin(115200);       // start serial comms at speed 115200
  Serial.println("\n\nWebserver demo sketch");
  
  // onboard LEDs
    pinMode(LEDpin, OUTPUT);
    digitalWrite(LEDpin, LOW);   
  
  // Connect to Wifi
    Serial.print("Connecting to ");
    Serial.println(SSID);
    WiFi.begin(SSID, PWD);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
    }
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());

  // set up web server pages to serve (API resources)
    server.on("/test", handleTest);               // '/test' URL is requested, run the procedure named 'handleTemp'
    server.on("/button", handleButton);           // '/button' URL requested
    server.on("/", handleRoot);                   // root web page requested
    server.onNotFound(handleNotFound);            // if invalid url is requested

  // start web server
    server.begin();    
    #if defined ESP32
        WiFi.setSleep(false);                     // stop the wifi being turned off if not used for a while  (esp32)
    #else
        WiFi.setSleepMode(WIFI_NONE_SLEEP);       // stop the wifi being turned off if not used for a while  (esp8266)
    #endif
    WiFi.mode(WIFI_STA);                          // turn off access point - options are WIFI_AP, WIFI_STA, WIFI_AP_STA or WIFI_OFF 
        
}  // setup



// ----------------------------------------------------------------


// this section runs repeatedly in a loop


void loop() {

  server.handleClient();                         // service any web page requests 

  digitalWrite(LEDpin, !digitalRead(LEDpin));    // invert onboard LED status 
  delay(200);                                    // wait 200 milliseconds
  
}  // loop



// ----------------------------------------------------------------
//      -test web page requested     i.e. http://x.x.x.x/
// ----------------------------------------------------------------
// demonstrate sending a plain text reply

void handleRoot(){

  if (serialDebug) Serial.println("Root page requested");

  String message = "root web page";

  server.send(404, "text/plain", message);   // send reply as plain text
  
}  // handleRoot



// ----------------------------------------------------------------
//      -test web page requested     i.e. http://x.x.x.x/test
// ----------------------------------------------------------------
// demonstrate sending html reply

void handleTest(){

  if (serialDebug) Serial.println("Test page requested");

  WiFiClient client = server.client();     // open link with client

  // html header
    client.print("<!DOCTYPE html> <html lang='en'> <head> <title>Web Demo</title> </head> <body>\n");         // basic html header

  // html body
    client.print("<h1>Test Page</h1>\n");

  // end html
    client.print("</body></html>\n");
    delay(3);
    client.stop();
  
}  // handleTest



// ----------------------------------------------------------------
//      -button web page requested     i.e. http://x.x.x.x/button
// ----------------------------------------------------------------
// demonstrate use of a html buttons

void handleButton(){

  if (serialDebug) Serial.println("Button page requested");
    
  // check if button1 has been pressed
    if (server.hasArg("button1")) {
        Serial.println("Button 1 was pressed");       
    }

  // check if button2 has been pressed
    if (server.hasArg("button2")) {
        Serial.println("Button 2 was pressed");       
    }

    
  // send reply to client
    
  WiFiClient client = server.client();     // open link with client

  // html header
    client.print("<!DOCTYPE html> <html lang='en'> <head> <title>Web Demo</title> </head> <body>\n");         // basic html header
    client.print("<FORM action='/button' method='post'>\n");       // used by the buttons in the html (action = the web page to send it to
    
  // html body
    client.print("<h1>Button demo page</h1>\n");
    if (server.hasArg("button1")) client.print("Button 1 has been pressed!");
    if (server.hasArg("button2")) client.print("Button 2 has been pressed!");
    client.print("<br><br><input style='height: 35px;' name='button1' value='Demo button 1' type='submit'> \n");
    client.print("<br><br><input style='height: 35px;' name='button2' value='Demo button 2' type='submit'> \n");

  // end html
    client.print("</body></html>\n");
    delay(3);
    client.stop();
  
}  // handleButton



// ----------------------------------------------------------------
//                      -invalid web page requested
// ----------------------------------------------------------------
// send this reply to any invalid url requested

void handleNotFound() {

  if (serialDebug) Serial.println("Invalid page requested");

  String tReply;
  
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



// ----------------------------------------------------------------------------------------------------------------


// Not used in this sketch but provided as an example of how to request an external web page and receive the reply as a string


// ----------------------------------------------------------------
//                        request a web page
// ----------------------------------------------------------------
//     parameters = ip address, page to request, port to use (usually 80), maximum chars to receive, ignore all in reply before this text 
//          e.g.   String reply = requestWebPage("192.168.1.166","/log",80,600,"");

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


// ----------------------------------------------------------------
// end
