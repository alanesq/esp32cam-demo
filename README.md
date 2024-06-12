
## ESP32Cam-demo sketch for use with the Arduino IDE 

Note: the new v3 of the esp32 board manager has kindly changed a lot of stuff resulting in a lot of sketches no longer working :-( <br>
I have now updated the sketch to work with both.   [more info here](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html) <br>


I show in this sketch how to use the esp32cam as easily as possible.  Everything I learn I try to add to it, please let me know if you have anything which you think can be added or changed to improve it - I am not a professional programmer so am sure there is plenty of room for improvement... <br>
This sketch has got a bit larger than I anticipated but this is just because it now has so many individual demonstrations of ways to use the camera, I have tried to make each part as easy to follow as possible with lots of comments etc.. <br>
The camera is not great quality and very poor in low light conditions but it is very cheap (around £5 each if buying several) and I think has lots of potential for interesting applications. <br>
This sketch is just a collection of all I have discovered/learned in the process of using them myself<br>

Note: This sketch now has the facility for OTA updates over the network, you need to copy the file ota.h in to your sketch folder and enable it in settings (#define ENABLE_OTA 1)
<br>
If you have issues with the camera keep stopping working etc. I have had a couple of these with dodgy camera modules so it is worth trying another one to see if this is the 
source of your problems. <br>

<table><tr>
  <td><img src="/images/root.png" /></td>
  <td><img src="/images/rgb.png" /></td>
  <td><img src="/images/grey.png" /></td>
</tr></table> 

This can be used as a starting point sketch for projects using the ESP32cam development board, it has the following features.
 - Web server with live video streaming and control buttons
 - SD card support (using 1-bit mode - gpio pins are usually 2, 4, 12 & 13 but using 1bit mode only uses pin 2) - I have heard there may be problems reading the sd card, I have only used it to write files myself?
 - Stores captured image on sd-card or in spiffs if no sd card is present
 - IO pins available for general use are 12 and 13 (12 must not be pulled high at boot)
 - Option to connect a MCP23017 chip to pins 12 and 13 to give you 16 gpio pins to use (this requires the Adafruit MCP23017 library)
 - The flash led is still available for use on pin 4 when using an sd card
 - PWM control of flash/illumination lED brighness implemented (i.e. to give brightness control)
 - Can read the image as RGB data  (i.e. 3 bytes per pixel for red, green and blue value)
 - Act as web client (reading the web page in to a string) - see requestWebPage()

The root web page uses AJAX to update info. on the page.  This is not done in the conventional way where variable data is passed but 
instead passes complete lines of text, it may not be elegant but it makes changing what information is displayed much easier as all you 
have to do is modify what info handleData() sends. 

BTW - I have created a timelapse sketch based on this one which may be of interest: https://github.com/alanesq/esp32cam-Timelapse

LATEST NEWS!!!
There is now a very cheap motherboard available for the esp32cam which make it as easy to use as any other esp development board. 
Search eBay for "esp32cam mb" - see http://www.hpcba.com/en/latest/source/DevelopmentBoard/HK-ESP32-CAM-MB.html 
It looks like the esp32cam suplied with them are not standard and have one of the GND pins modified to act as a reset pin?
So on esp32cam modules without this feature you have to plug the USB in whilst holding the program button to upload a sketch 
I find I have to use the lowest serial upload speed or it fails (Select 'ESP32 dev module' in the Arduino IDE to have the option and 
make sure PSRam is enabled). 
The wifi can be very poor whilst in the motherboard (I find this happens if you have something near the antenna on the esp32cam modules) 
but if I rest my thumb above the antenna I find the signal works ok).  
There is also now a esp32cam with built in USB available called the "ESP32-CAM-CH340" 
Many of the ebay listing include an external antenna and I would suggest this would be a good option if ordering one.  

I have tried to make the sketch as easy to follow/modify as possible with lots of comments etc. and no additional libraries used, 
as I found it quiet confusing as an amateur trying to do much with this module and difficult to find easy to understand 
examples/explanations of what I wanted to do, so I am publishing this sketch in the hope it will encourage/help others to have a 
try with these powerful and VERY affordable modules.
The greyscale procedure I think is the most interesting as it shows how to switch camera modes and process the raw data very well.
BTW - For some examples of serving web pages with en ESP module you may like to have a look 
      at: https://github.com/alanesq/BasicWebserver/blob/master/misc/VeryBasicWebserver.ino

BTW - Even if you do not require the camera I think these modules have some uses in many projects as they are very cheap, have a 
built in sd card reader, 
bright LED and the 4mb psram could prove useful for storing large amounts of temp data etc?   (see the RGB section of the code to 
see how it can be used).

created using the Arduino IDE with ESP32 module installed  
(See https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/)
No additional libraries required

[Youtube video on using the ESP32Cam board](https://www.youtube.com/watch?v=FmlxC0goKew)

[Schematic](https://github.com/SeeedDocument/forum_doc/blob/master/reg/ESP32_CAM_V1.6.pdf)

[Info on camera settings](https://randomnerdtutorials.com/esp32-cam-ov2640-camera-settings/)


----------------

How to use:

Enter your network details (ssid and password in to the sketch) and upload it to your esp32cam module
If you monitor the serial port (speed 15200) it will tell you what ip address it has been given.
If you now enter this ip address in to your browser you should be greated with the message "Hello from esp32cam"

If you now put /stream at the end of the url      i.e.   http://x.x.x.x/stream
It will live stream video from the camera

If you have an sd card inserted then accessing    http://x/x/x/x/photo
Will capture an image and save it to the sd card

There is a procedure which demonstrates how to get RGB data from an image which will allow for processing the images 
as data (http://x.x.x.x/rgb).

URLs:
<br>http://x.x.x.x/              Hello message
<br>http://x.x.x.x/jpg           capture image and display as a JPG
<br>http://x.x.x.x/jpeg          as above but refreshes every 2 seconds    
<br>http://x.x.x.x/photo         Capture an image and save to sd card
<br>http://x.x.x.x/stream        Show live streaming video
<br>http://x.x.x.x/img           Show most recent image saved to sd card
<br>http://x.x.x.x/img?img=1     Show image number 1 on sd card
<br>http://x.x.x.x/greyscale     Show how to capture a greyscale image and look at the raw data
<br>http://x.x.x.x/rgb           Captures an image and converts to RGB data (will not work with the highest 
                                 resolution images as there is not enough memory)
                                           
GPIO PINS:
<br>    13      free (used by sd card but free if using 1 bit mode)
<br>    12      free (must be low at boot, used by sd card but free if using 1 bit mode)
<br>    14      used by sd card (usable is SPI clock?)
<br>    2       used by sd card (usable as SPI MISO?)
<br>    15      used by sd card (usable as SPI CS?)
<br>    1       serial - output only?
<br>    3       serial - input only?
<br>    4       has the illumination/flash led on it - led could be removed and use as output?
<br>    33      onboard led - use as output?

Some great info here:   https://github.com/raphaelbs/esp32-cam-ai-thinker/blob/master/docs/esp32cam-pin-notes.md
BTW-You can use an MCP23017 io expander chip on pins 12 and 13 to give you 16 general purpose gpio pins, this requires the adafruit MCP23017 library to be installed.
Note: I have been told there may be issues reading files when sd-card is in 1-bit mode, I have only used it for writing them myself.


----------------

Notes
-----

See the test procedure at the end of the sketch for several demos of what you can do

You can see an example Processing sketch for displaying the raw rgb data from this sketch
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

When streaming video these units can get very hot so if you plan to do a lot of streaming this is worth checking to make sure it is not going to over heat.

[Demo of accessing grayscale image data](https://github.com/alanesq/esp32cam-demo/blob/master/Misc/esp32camdemo-greyscale.ino)
    
[sketch which can record AVI video to sd card](https://github.com/mtnbkr88/ESP32CAMVideoRecorder)

[Handy way to upload images to a computer/web server via php which is very reliable and easy to use](https://RandomNerdTutorials.com/esp32-cam-post-image-photo-server/)

[How to crop images on the ESP32cam](https://makexyz.fun/esp32-cam-cropping-images-on-device/)

[Some good info here](https://github.com/raphaelbs/esp32-cam-ai-thinker) - [and here](https://randomnerdtutorials.com/projects-esp32-cam/)

[Wokwi - handy for testing out bits of ESP32 code](https://wokwi.com/)

Some sites I find handy when creating HTML:
 - [test html](https://www.w3schools.com/tryit/tryit.asp?filename=tryhtml_hello )
 - [check html for errors](http://www.freeformatter.com/html-formatter.html#ad-output)
 - [learn HTML](https://www.w3schools.com/)
      
You may like to have a play with a Processing sketch I created which could be used to grab JPG images from this camera and motion detect: 
https://github.com/alanesq/imageChangeMonitor  

I have a demo sketch of how to capture and save a raw RGB file (see comments at top of how you can view the resulting file)
https://github.com/alanesq/misc/blob/main/saveAndViewRGBfiles.ino

If you have any handy info, tips, or improvements to my code etc. please feel let me know at: alanesq@disroot.org




                                                      https://github.com/alanesq/esp32cam-demo
