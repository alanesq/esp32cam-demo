<h1>ESP32Cam-demo sketch for use with the Arduino IDE</h1>
<p align="center"><img src="/images/esp32cam.jpeg" width="80%"/></p>

LATEST NEWS!!!
There is now a very cheap motherboard available for the esp32cam which make it as easy to use as any other esp development board. 
Search eBay for "esp32cam mb" - see http://www.hpcba.com/en/latest/source/DevelopmentBoard/HK-ESP32-CAM-MB.html 
It looks like older esp32cam modules the reset button has no effect on them (I am guessing this is a feature on newer modules) so on 
these you have to plug the USB in whilst holding the program button to upload a sketch also I find I have to use the lowest upload 
speed or it fails to upload.  The wifi is very poor whilst in the motherboard (I find this happens if you have something near the antenna on the esp32cam modules) but if I rest my thumb above the antenna I find the signal works great).  

<pre>
This can be used as a starting point sketch for projects using the esp32cam development board, 
it has the following features:
-web server with live video streaming and control buttons
-sd card support (using 1-bit mode - data pins are usually 2, 4, 12 & 13 but using 1bit mode only uses pin 2)
-stores captured image in spiffs if no sd card is present
-io pins available for use are 12 and 13 (12 must not be pulled high at boot)
-Option to connect a MCP23017 chip to pins 12 and 13 to give you 16 gpio pins to use (this requires the Adafruit MCP23017 library)
-flash led is still available for use on pin 4 when using an sd card
-PWM control of flash/illumination lED brighness implemented (i.e. to give brighness control)
-Can read the image as RGB data  (i.e. 3 bytes per pixel for red green and blue value)
-Act as web client (reading the web page in to a string) - see requestWebPage()
 </pre>
I have tried to make the sketch as easy to follow/modify as possible with lots of comments etc. and no additional libraries used, as I found it quiet confusing as an ameteur trying to do much with this module and difficult to find easy to understand examples/explanations of what I wanted to do, so I am publishing this sketch in the hope it will encourage/help others to have a try with these powerful and VERY affordable modules.

BTW - Even if you do not require the camera I think these modules have some uses in many projects as they are very cheap, have a built in sd card reader, 
bright LED and the 4mb psram could prove useful for storing large amounts of temp data etc?   (see the RGB section of the code to see how it can be used).

created using the Arduino IDE with ESP32 module installed  (See https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/)
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

There is a procedure which demonstarates how to get RGB data from an image which will allow for processing the images as data (http://x.x.x.x/rgb).

<pre>
URLs:
http://x.x.x.x/              Hello message
http://x.x.x.x/photo         Capture an image and save to sd card
http://x.x.x.x/stream        Show live streaming video
http://x.x.x.x/img           Show most recent image saved to sd card
http://x.x.x.x/img?img=1     Show image number 1 on sd card
http://x.x.x.x/rgb           Captures an image and converts to RGB data (will not work with the highest 
                             resolution images as there is not enough memory)
</pre>

----------------

Notes
-----

This looks like it may contain useful info. on another way of getting RGB data from the camera: https://eloquentarduino.github.io/2020/01/image-recognition-with-esp32-and-arduino/

These modules require a good power supply.  I find it best to put a good sized smoothing capacitor across the supply as the wifi especially can put lots 
of spikes on the line.
If you get strange error messages, random reboots, wifi dropping out etc. first thing to do is make sure it is not just a power problem.

BTW - You may like to have a look at the security camera sketch I have created as this has lots more going on including FTP, email, OTA updates
https://github.com/alanesq/CameraWifiMotion

I have heard reports of these modules getting very warm when in use although I have not experienced this myself, I suspect it may be when streaming video for long periods?  May be worth bearing in mind.

GPIO:
io pins available for use are 13 and 12 (12 must not be high at boot)
pin 16 is used for psram but you may get away with using it as input for a button etc.
You could also use 1 and 3 if you do not use Serial or 14,2&15 if not using SD Card
More info: https://randomnerdtutorials.com/esp32-cam-ai-thinker-pinout/
You can use a MCP23017 io expander chip to give 16 gpio lines by enabling 'useMCP23017' in the setup section and connecting
  the i2c pins to 12 and 13 on the esp32cam module.  Note: this requires the adafruit MCP23017 library to be installed.
    


                                                                https://github.com/alanesq/esp32cam-demo
