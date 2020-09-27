
<p align="center"><img src="/images/esp32cam.jpeg" width="30%"/></p>

# esp32cam-demo
esp32cam module demo / project starting point sketch using Arduino ide

This can be used as a starting point sketch for your projects using the esp32cam development board, it has the following features
 *        web server with live video streaming and control buttons
 *        sd card support (using 1bit mode to free some io pins)
 *        io pins available for use are 12 and 13 (must be low at boot)
 *        flash led is still available for use on pin 4 when using sd card
 
 I found it quiet confusing trying to do much with this module and so have tried to compile everything I have learned so far in to 
 a easy to follow sketch to encourage others to have a try with these powerful and VERY affordable modules...
 
The module is not the easiest to use as it does not have a usb socket on board so you will need an in circuit programmer to program it, these can be bought on eBay very cheaply.  I built myself a "shield" from perf board which I can plug the esp32cam module and programmer in to and using a couple of toggle switches switch the module on and off and select programming mode.  This makes using these boards much more convenient - I think there is a gap in the market for someone to produce these and sell on eBay ;-)

created using the Arduino IDE with ESP32 module installed   (https://dl.espressif.com/dl/package_esp32_index.json)
No additional libraries required

----------------

How to use:

Enter your network details (ssid and password in to the sketch) and upload it to your esp32cam module
If you monitor the serial port (speed 15200) it will tell you what ip address it has been given.
If you now enter this ip address in to your browser you should be greated with the message "Hello from esp32cam"

If you now put /stream at the end of the url      i.e.   http://x.x.x.x/stream
It will live stream video from the camera

If you have an sd card inserted then accessing    http://x/x/x/x/photo
Will capture an image and save it to the sd card

URLs:
http://x.x.x.x/              Hello message
http://x.x.x.x/photo         Capture an image and save to sd card
http://x.x.x.x/stream        Show live streaming video
http://x.x.x.x/img           Show most recent image saved to sd card
http://x.x.x.x/img?img=1     Show image number 1 on sd card


----------------

BTW - You m ay like to have a look at the security camera sketch I have created as this has lots more going on including FTP, email, OTA updates
https://github.com/alanesq/CameraWifiMotion

