

NOTE: When using Arduino IDE I find it reboots if no sd card present and the LED stays on full all the time with latest esp32 board manager.  I am using version  1.0.6<br>
known bug: https://github.com/espressif/arduino-esp32/issues/5195<br><br>
      
<h1>ESP32Cam-demo sketch for use with the Arduino IDE or PlatformIO</h1>

I try to show in this sketch how to use the esp32cam as easily as possible.  Everything I learn I try to add to it, please let me know if you have anything which you think can be added or changed to improve it<br>

<table><tr>
  <td><img src="/images/root.png" /></td>
  <td><img src="/images/rgb.png" /></td>
</tr></table> 

This can be used as a starting point sketch for projects using the esp32cam development board, 
it has the following features:
<br>-Web server with live video streaming and control buttons
<br>-SD card support (using 1-bit mode - gpio pins are usually 2, 4, 12 & 13 but using 1bit mode only uses pin 2) - I have heard there may be problems reading the sd card, I have only used it to write files myself?
<br>-Stores captured image on sd-card or in spiffs if no sd card is present
<br>-IO pins available for general use are 12 and 13 (12 must not be pulled high at boot)
<br>-Option to connect a MCP23017 chip to pins 12 and 13 to give you 16 gpio pins to use (this requires the Adafruit MCP23017 library)
<br>-The flash led is still available for use on pin 4 when using an sd card
<br>-PWM control of flash/illumination lED brighness implemented (i.e. to give brighness control)
<br>-Can read the image as RGB data  (i.e. 3 bytes per pixel for red, green and blue value)
<br>-Act as web client (reading the web page in to a string) - see requestWebPage()

<br>The root web page uses AJAX to update info. on the page.  This is done in the easiest to understand/follow way I can, it seems a bit complicated 
<br>at first but basically you just create html with a named <span> where you want the info to go, the javascript then periodically sends a request to <br>http://x.x.x.x/data which returns the information seperated by commas which the JavaScript splits in to an array and then sends to the relevant 
<br><span> on the web page.

<br>BTW - I have created a timelapse sketch based on this one which may be of interest: https://github.com/alanesq/esp32cam-Timelapse

<br><br>LATEST NEWS!!!
<br>There is now a very cheap motherboard available for the esp32cam which make it as easy to use as any other esp development board. 
<br>Search eBay for "esp32cam mb" - see http://www.hpcba.com/en/latest/source/DevelopmentBoard/HK-ESP32-CAM-MB.html 
It looks like the esp32cam suplied with them are not standard and have one of the GND pins modified to act as a reset pin?<br>
So on esp32cam modules without this feature you have to plug the USB in whilst holding the program button to upload a sketch 
I find I have to use the lowest serial upload speed or it fails (Select 'ESP32 dev module' in the Arduino IDE to have the option and 
make sure PSRam is enabled). <br>
There is also now a esp32cam with built in USB available called the "ESP32-CAM-CH340".<br>
The wifi is very poor whilst in the motherboard (I find this happens if you have something near the antenna on the esp32cam modules) <br>
but if I rest my thumb above the antenna I find the signal works ok). <br> 
Many of the ebay listing include an external antenna and I would suggest this would b a good option if ordering one.  <br>
So they are far from perfect but still for the price I think well worth having.
<br><br><br>

I have tried to make the sketch as easy to follow/modify as possible with lots of comments etc. and no additional libraries used, 
as I found it quiet confusing as an ameteur trying to do much with this module and difficult to find easy to understand 
examples/explanations of what I wanted to do, so I am publishing this sketch in the hope it will encourage/help others to have a 
try with these powerful and VERY affordable modules.
BTW - For some examples of serving web pages with en ESP module you may like to have a look 
      at: https://github.com/alanesq/BasicWebserver/blob/master/misc/VeryBasicWebserver.ino

BTW - Even if you do not require the camera I think these modules have some uses in many projects as they are very cheap, have a 
built in sd card reader, 
bright LED and the 4mb psram could prove useful for storing large amounts of temp data etc?   (see the RGB section of the code to 
see how it can be used).

created using the Arduino IDE with ESP32 module installed  
(See https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/)
No additional libraries required


BTW - If you has an ideas/info./suggestions to improve this sketch etc. please let me know.

There is a very good Youtube video on using the ESP32Cam board here: https://www.youtube.com/watch?v=FmlxC0goKew&t=610s

----------------

How to use:

Enter your network details (ssid and password in to the sketch) and upload it to your esp32cam module
If you monitor the serial port (speed 15200) it will tell you what ip address it has been given.
If you now enter this ip address in to your browser you should be greated with the message "Hello from esp32cam"

If you now put /stream at the end of the url      i.e.   http://x.x.x.x/stream
It will live stream video from the camera

If you have an sd card inserted then accessing    http://x/x/x/x/photo
Will capture an image and save it to the sd card

There is a procedure which demonstarates how to get RGB data from an image which will allow for processing the images 
as data (http://x.x.x.x/rgb).

<br>URLs:
<br>http://x.x.x.x/              Hello message
<br>http://x.x.x.x/photo         Capture an image and save to sd card
<br>http://x.x.x.x/stream        Show live streaming video
<br>http://x.x.x.x/img           Show most recent image saved to sd card
<br>http://x.x.x.x/img?img=1     Show image number 1 on sd card
<br>http://x.x.x.x/rgb           Captures an image and converts to RGB data (will not work with the highest 
                             resolution images as there is not enough memory)
                                           
GPIO PINS:
<br>The available pins are very limited on the esp32cam, my best understanding is:
<br>If using the sd card (must be in 1-bit mode) the main io pins available for general use are 13 and 12 
<br>(12 must not be high at boot), 14, 2 & 15 should also be available if you are not using the SD Card.
<br>You can also use pins 1 and 3 if you do not use Serial.
<br>Pin 4 is the onboard flash LED which you could possibly remove and use this pin for i/o but again sd card 
<br>needs to be in 1-bit mode if you are using it otherwise the sd-card uses pin 4. pin 33 is another onboard 
<br>led which could possibly be used?
<br>More info: https://randomnerdtutorials.com/esp32-cam-ai-thinker-pinout/
<br>BTW-You can use an MCP23017 io expander chip on pins 12 and 13 to give you 16 general purpose gpio pins, this requires the adafruit MCP23017 library to be installed.
<br>Pin 16 is used for psram but you may get away with using it as input for a button etc.?
<br>Note: I have been told there may be issues reading files when sd-card is in 1-bit mode, I have only used it for writing them myself.

----------------

Notes
-----

See the test procedure at the end of the sketch for several demos of what you can do

You can see an example Processing sketch for displaying the raw rgb data from this sketch<br>
here: https://github.com/alanesq/esp32cam-demo/blob/master/Misc/displayRGB.pde
This would read in a file created from the Arduino command:   client.write(rgb, ARRAY_LENGTH);
You can create such a file by setting the 'sendRGBfile' flag in settings and then accessing the /rgb page

This looks like it may contain useful info. on another way of getting RGB data from the camera: 
   https://eloquentarduino.github.io/2020/01/image-recognition-with-esp32-and-arduino/

These modules require a good power supply.  I find it best to put a good sized smoothing capacitor across the 
upply as the wifi especially can put lots 
of spikes on the line.
If you get strange error messages, random reboots, wifi dropping out etc. first thing to do is make sure it is 
not just a power problem.

BTW - You may like to have a look at the security camera sketch I have created as this has lots more going on 
including FTP, email, OTA updates
https://github.com/alanesq/CameraWifiMotion

I have heard reports of these modules getting very warm when in use although I have not experienced this myself, 
I suspect it may be when streaming video for long periods?  May be worth bearing in mind.

You can see a demo of accessing greyscale image data here:
    https://github.com/alanesq/esp32cam-demo/blob/master/Misc/esp32camdemo-greyscale.ino
    
A very impressive sketch here which can record AVI video to sd card - https://github.com/mtnbkr88/ESP32CAMVideoRecorder

A handy way to upload images to a computer/web server via php which is very reliable and easy to use: https://RandomNerdTutorials.com/esp32-cam-post-image-photo-server/

How to crop images on the esp32cam: https://makexyz.fun/esp32-cam-cropping-images-on-device/

Some good info here: https://github.com/raphaelbs/esp32-cam-ai-thinker

If you have any handy info, tips, or improvements to my code etc. please feel let me know at: alanesq@disroot.org




                                                      https://github.com/alanesq/esp32cam-demo
