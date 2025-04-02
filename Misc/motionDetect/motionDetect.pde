/*
 ----------------------------------------------------

 ESP32cam motion detection using Processing - 02Apr25

        uses libraries OpenCV and Minim

 ----------------------------------------------------

 Notes: 
     Convert jpg files to amovie:  convert -delay 10 *.jpg output.mp4
     Set up individual camera settings in 'setCam()'

*/

// ----------------------------------------------------
//                       Settings
// ----------------------------------------------------

  String myParam = "ESPCam";                                          // camera title
  String imgUrl = "http://192.168.1.2/jpg" + "?image.jpg";            // url of the esp32cam 
  String imageFolder = sketchPath() + "/images/";                     // folder to store images
  
  boolean showDiags = true;            // show extra diagnostic information on screen
  int lineSpace = 20;                  // line spacing for text/buttons on screen
  boolean soundEnabled = false;        // if sound when motion detected
  boolean UDPenabled = false;          // if sending UDP broadcasts is enabled
  int minTriggerTime = 15;             // Minimum time between repeat triggers of sound or UDP (seconds)
  int maxLineTriggers = 40;            // If trigger level of a horizontal line excedes this it is ignored (percentage 0-100)
  int triggerLevel = 65;               // movement trigger level above baseline level
  int enableAdaptiveTriggerLevel = 1;  // if trigger level adapts to previous movement levels (1 or 0)
  int windowSize = 50;                 // Sliding window size for average calculation
  long imageAgeLimit = 7;              // days to keep stored images
  int resizeX = 160;                   // size of image created to motion detect
  int resizeY = 120; 
  int changeThreshold = 20;            // Pixel intensity change threshold
  int delay = 800;                     // Delay in milliseconds between draw calls (ms)
  float alpha = 0.1;                   // Smoothing factor (0.0 < alpha <= 1.0)
  int graphHeight = 80;                // Height of the graph area
  int attemptsToTry = 5;               // Max retries when capturing image
  int overlayTint = 80;                // how srong the overlaed movement/mask is on the image (0-255)


  // A 4x4 movement detection mask (true = ignore area) - masked area is shown as yellow
      boolean[][] mask = {
        { false, false, false, false },        // Top row
        { false, false, false, false },
        { false, false, false, false },
        { false, false, false, false }         // Bottom row
      };


// ----------------------------------------------------

// required to broadcast UDP
    import java.net.*;
    import java.io.*;
    DatagramSocket socket;
    InetAddress broadcastAddress;
    int port = 12345;

// required to delete old image files
  import java.io.File;
  import java.util.Date;

// diag variables
    int lineErrorCounter = 0;     // for monitoring how many horizontal line rejections are occuring 
    int maxHLTriggers = 0;        // max triggers on a horizontal line
    long fameCompareTime = 0;     // timing the frame compare procedure
    long fameCaptureTime = 0;     // timing the frame capture from URL
    int TriggerCounter = 0;       // trigger counter
    
// control buttons
  int buttonWidth = 70;
  int buttonHeight = 17;
  int buttonSpacing = 80;    // horizontal spacing of the buttons
  int buttonY = 3 * lineSpace - buttonHeight + 4;   // Y position of the buttons
  int soundButtonX = 5;      // X position of the sound button
  int UDPbuttonX = soundButtonX + buttonSpacing;         // X position of the sound button
  int imageButtonX = soundButtonX + buttonSpacing * 2;   // X position of the save image button  
  

// ----------------------------------------------------
//                    camera settings
// ----------------------------------------------------  
// camera selected from command line parameter

void setCam(String param) {

    if (param.equals("front")) {
        imgUrl = "http://192.168.1.144/jpg" + "?image.jpg"; 
        imageFolder = sketchPath() + "/../images/front/";     
        // change mask
          mask[3][0] = true; mask[3][1] = true; mask[3][2] = true; mask[3][3] = true;     
          mask[0][3] = true; mask[1][3] = true; mask[2][3] = true;          
    } 
    
    if (param.equals("side")) {
        imgUrl = "http://192.168.1.192/jpg" + "?image.jpg"; 
        imageFolder = sketchPath() + "/../images/side/";                
    }   
    
    if (param.equals("back")) {
        imgUrl = "http://192.168.1.222/jpg" + "?image.jpg"; 
        imageFolder = sketchPath() + "/../images/back/";     
        // change mask
          //mask[0][0] = true; mask[0][1] = true; mask[0][2] = true; mask[0][3] = true; 
          //mask[1][3] = true; 
    }        
    
    deleteOldFiles(imageFolder);        // delete any image files older than 2 weeks 
}


PImage currentImg, previousImg, motionOverlay;
int lastRun = 0;                 // time the daily deletion of older images was last performed
int saveThreshold = 1000000 - triggerLevel;  // self adaptive movement trigger level
int refreshRate = 4;             // frame rate
int lastDrawTime = 0;            // Stores the last time draw() was called
int movementLevel = 0;           // Accumulated movement level
ArrayList<Integer> recentReadings = new ArrayList<Integer>(); // Store past readings
int maxReading = 0;              // Maximum reading for bar graph normalization
String lastDownloadTime = "";    // Store last image fetch time
long lastUDPtrigger = 0;         // Last time a UDP broadcast was sent
long lastSoundtrigger = 0;       // Last time a sound was triggered

// sound
import ddf.minim.*;
Minim minim;
AudioPlayer beep;


// ----------------------------------------------------
//                       -setup
// ----------------------------------------------------

void setup() {

  // audio
    minim = new Minim(this);
    beep = minim.loadFile("../beep.wav");  
    
  // get camera from parameter
    myParam = System.getenv("motionCAM");
    if (myParam == null) myParam = "side";    // default camera
    println(getCurrentDateTime() + "[camera] motionCAM parameter: " + myParam);  
    setCam(myParam);    // set camera parameters
    
  // setup display
      frameRate(refreshRate);
      size(640, 480);
      surface.setResizable(true);    
      surface.setTitle("Motion: " + myParam);
  
  println(getCurrentDateTime() + "[camera] '" + myParam + "' starting");
  
  // request image from camera
    currentImg = requestImg();            // load first image       
    normalizeBrightness(currentImg);      // adjust brigtness to compensate for sudden changes in sunlight
    if (currentImg != null) previousImg = currentImg.get();    // store this image as the reference for comparison
    
  if (soundEnabled == true) makeSound();  
  
  if (enableAdaptiveTriggerLevel == 0) saveThreshold = 0;        // if adaptive trigger is disabled
  
// setup for UDP broadcasting
  try {
    socket = new DatagramSocket(null); 
    socket.setReuseAddress(true);   // Enable reuse
    socket.setBroadcast(true);      // Enable broadcast
    socket.bind(new InetSocketAddress(port));
    broadcastAddress = InetAddress.getByName("192.168.1.255");
  } catch (Exception e) {
    e.printStackTrace();
  }  
  sendUDPmessage("GM:Camera " + myParam + " starting");       // send a UDP broadcast
}


// ----------------------------------------------------
//                       -draw
// ----------------------------------------------------

void draw() {
  // delay each time to ensure the camera is not overloaded
  if (millis() - lastDrawTime >= delay) {
  
      lastDrawTime = millis(); // Update the last draw time 
 
      // Refresh image
        if (currentImg != null) previousImg = currentImg.get();
        currentImg = requestImg();    
        normalizeBrightness(currentImg);      // adjust brigtness to compensate for sudden changes in sunlight
        
      if (previousImg != null && currentImg != null) {
      
        int movementLevel = compareImages(previousImg, currentImg, mask);

        // Update graph data
          recentReadings.add(movementLevel);
          if (recentReadings.size() > windowSize) {
            recentReadings.remove(0);
          }
          maxReading = max(maxReading, movementLevel);

        // display perameters
          background(0);
          tint(255, 255);    
          fill(0, 0, 255);    

        // Display new image
          image(currentImg, 0, 0, width, height);
    
        // display text
            textSize(18);
            textAlign(LEFT);
            text(myParam + ": " + lastDownloadTime, 10, 1 * lineSpace);
            text("Movement: " + movementLevel + "   Trigger: " + (saveThreshold + triggerLevel), 10, 2 * lineSpace);
            
            // if extra diagnostic info display is enabled
            if (showDiags == true) {      
                text("Line rejections: " + lineErrorCounter + " (" + maxHLTriggers + "/" + resizeX + ")", width / 2, 1 * lineSpace);
                text("Triggers: " + TriggerCounter, width / 2, 2 * lineSpace);
                text("Time to capture image: " + fameCaptureTime + "ms", width / 2, 3 * lineSpace);
                text("Time to compare images: " + fameCompareTime + "ms", width / 2, 4 * lineSpace);
            }
            
        // if movement threshold exceeded
          if (enableAdaptiveTriggerLevel == 1) updateThreshold();   // adapt threshold level
          if (movementLevel > saveThreshold + triggerLevel) {
            println(getCurrentDateTime() + "[camera] Movement detected (" + myParam +")");
            //currentImg.save(imageFolder + lastDownloadTime + ".jpg"); 
            save(imageFolder + lastDownloadTime + ".jpg"); 
            if (soundEnabled == true) makeSound();
            if (UDPenabled == true) {
                sendUDPmessage("IN:Movement detected");      // send UDP broadcast
            }
            TriggerCounter++;      // trigger counter for extra diag display
          }
           
        // Draw movement graph
          tint(255, 80); 
          drawGraph();          
          
        // display movement detection image on top of camera image 
          tint(255, 255); 
          image(motionOverlay, 0, 0, width, height);  
        
        // delete older images once per day
          if ((millis() - lastRun) % (24 * 60 * 60 * 1000) == 0) {
            deleteOldFiles(imageFolder);
            lastRun = millis(); // update the last run time
          }
    }
  }
  togButton();        // Sound on/off toggle button
  saveButton();       // save image button
  
}   // draw


// ----------------------------------------------------
//         sound and UDP on/off toggle buttons
// ----------------------------------------------------

void togButton() {
    // Draw the sound toggle button
        if (soundEnabled) {
          fill(0, 255, 0); // Green when ON
        } else {
          fill(255, 0, 0); // Red when OFF
        }
        tint(255, 64); 
        rect(soundButtonX, buttonY, buttonWidth, buttonHeight); 
        
    // Draw the UDP toggle button
        if (UDPenabled) {
          fill(0, 255, 0); // Green when ON
        } else {
          fill(255, 0, 0); // Red when OFF
        }
        tint(255, 64); 
        rect(UDPbuttonX, buttonY, buttonWidth, buttonHeight);         
         
    // Draw sound button label
        fill(0, 0, 255);   // blue
        tint(255, 64); 
        textSize(12);
        textAlign(CENTER, CENTER);
        text(soundEnabled ? "Sound ON" : "Sound OFF", soundButtonX + buttonWidth / 2, buttonY + buttonHeight / 2);
        
    // Draw UDP button label
        fill(0, 0, 255);   // blue
        tint(255, 64); 
        textSize(12);
        textAlign(CENTER, CENTER);
        text(UDPenabled ? "UDP ON" : "UDP OFF", UDPbuttonX + buttonWidth / 2, buttonY + buttonHeight / 2);        
}


// ----------------------------------------------------
//                  save image button
// ----------------------------------------------------

void saveButton() {
    // Draw the save button
        fill(0, 255, 0); // Green when ON
        tint(255, 64); 
        rect(imageButtonX, buttonY, buttonWidth, buttonHeight);  
    // Draw button label
        fill(0, 0, 255);   // blue
        tint(255, 64); 
        textSize(12);
        textAlign(CENTER, CENTER);
        text("Save", imageButtonX + buttonWidth / 2 , buttonY + buttonHeight / 2);
}
 
 
 
// ----------------------------------------------------
//          if mouse was clicked action buttons
// ---------------------------------------------------- 

void mousePressed() {    
  if (mouseX > soundButtonX && mouseX < soundButtonX + buttonWidth && mouseY > buttonY && mouseY < buttonY + buttonHeight) {
    soundEnabled = !soundEnabled; // Toggle the flag
  }
  if (mouseX > UDPbuttonX && mouseX < UDPbuttonX + buttonWidth && mouseY > buttonY && mouseY < buttonY + buttonHeight) {
    UDPenabled = !UDPenabled; // Toggle the UDP broadcasts flag
  }  
  if (mouseX > imageButtonX && mouseX < imageButtonX + buttonWidth && mouseY > buttonY && mouseY < buttonY + buttonHeight) {
    save(imageFolder + lastDownloadTime + ".jpg");    // save image
  }  
}  


// ----------------------------------------------------
//                load image from camera
// ----------------------------------------------------

PImage requestImg() {
  int startTime3 = millis();    // used to time this procedure
  
  for (int attempt = 1; attempt <= attemptsToTry; attempt++) {
    PImage img = requestImage(imgUrl);  // Start loading the image asynchronously
    int startTime2 = millis();
    boolean loaded = false;
    
    // Wait (up to 5 seconds) for the image to load without blocking the main thread
    while (millis() - startTime2 < 5000) {
      if (img.width > 1 && img.height > 1) {
        loaded = true;
        break;
      }
      try {
        Thread.sleep(50);
      } catch (InterruptedException e) {
        println(getCurrentDateTime() + "[camera] '" + myParam + "' interrupted: " + e.getMessage());
        return null;
      }
    }
    
    if (loaded) {
      lastDownloadTime = day() + "-" + month() + "-" + year() + "--" +
                         hour() + ":" + nf(minute(), 2) + ":" + nf(second(), 2);
      if (attempt > 1)
        println(getCurrentDateTime() + "[camera] '" + myParam + "' image captured ok on attempt " + attempt);
        fameCaptureTime = millis() - startTime3;    // store time to compare images
      return img;
    }
    else {
      println(getCurrentDateTime() + "[camera] '" + myParam + "' image capture timed out on attempt " + attempt);
    }
    
    // Cooldown before retrying
    try {
      Thread.sleep(800);
    } catch (InterruptedException e) {
      println(getCurrentDateTime() + "[camera] '" + myParam + "' interrupted during cooldown: " + e.getMessage());
      return null;
    }
  }
  
  println(getCurrentDateTime() + "[camera '" + myParam + "' - Failed to fetch image after " + attemptsToTry + " attempts");
  exit();        // close app 
  return null;
}



// ----------------------------------------------------
//                  compare two images
// ----------------------------------------------------
// a mask in the form of a 4x4 grid can be supplied (true = ignore area)

int compareImages(PImage img1, PImage img2, boolean[][] mask) {
  long startTime4 = millis();    // used to time this procedure
  maxHLTriggers = 0;          // maximum triggers on a horizontal line
  
  // Resize images for faster computation
  PImage smallImg1 = img1.get(); // Make a copy of img1
  PImage smallImg2 = img2.get(); // Make a copy of img2
  smallImg1.resize(resizeX, resizeY); // Resize modifies the image in place
  smallImg2.resize(resizeX, resizeY); // Resize modifies the image in place

  smallImg1.loadPixels();
  smallImg2.loadPixels();

  motionOverlay = createImage(smallImg1.width, smallImg1.height, ARGB);
  motionOverlay.loadPixels();

  int movementLevel = 0;

  int gridWidth = smallImg1.width / 4;
  int gridHeight = smallImg1.height / 4;
  int startOfLineTriggers = 0;

  for (int y = 0; y < smallImg1.height; y++) {
    startOfLineTriggers = movementLevel;      // store how many triggers at start of this horizontal line
    for (int x = 0; x < smallImg1.width; x++) {
      int i = x + y * smallImg1.width;

      // Determine which grid cell the pixel belongs to
      int gridX = x / gridWidth;
      int gridY = y / gridHeight;

      // Check if the mask excludes this cell
      if (mask != null && mask[gridY][gridX]) {
        motionOverlay.pixels[i] = color(128, 128, 0, overlayTint);   // highlight mask in yellow
        continue;                                           // skip detection for this pixel
      }

      float diff = brightness(smallImg1.pixels[i]) - brightness(smallImg2.pixels[i]);
      if (abs(diff) > changeThreshold) {                    // if motion detected
        movementLevel++;                                    // increment movement level detected value
        motionOverlay.pixels[i] = color(0, 128, 0, overlayTint);     // Highlight motion in green
      } else {
        motionOverlay.pixels[i] = color(0, 0, 0, 0);        // clear pixel
        //motionOverlay.pixels[i] = smallImg1.pixels[i];    // Keep original pixel
      }
    }  // x
    
    // if whole line is triggered then assune it is error (interference in image or whole image has changed)
      int Triggers = movementLevel - startOfLineTriggers;     // triggers on this line
      if (Triggers > (smallImg1.width * maxLineTriggers) / 100) {
        movementLevel = startOfLineTriggers;   // discard this line as error
        lineErrorCounter++;     // diag variable for monitoring line rejections
      }
      if (Triggers > maxHLTriggers) maxHLTriggers = Triggers;   // update diag variable maximum triggers per line 
      
  }  // y

  motionOverlay.updatePixels();

  // Display the image with motion highlighted
  image(motionOverlay, 0, 0, width, height);
  
  // store time to compare images
    fameCompareTime = millis() - startTime4;

  return movementLevel;
}


// ----------------------------------------------------
// normalise image brightness (to compensate for sun brightness changes)
// ----------------------------------------------------

void normalizeBrightness(PImage img) {
  
  if (img == null) return;
  img.loadPixels();
  float totalBrightness = 0;
  int numPixels = img.pixels.length;

  for (color c : img.pixels) {
    totalBrightness += brightness(c);
  }

  float avgBrightness = totalBrightness / numPixels;
  float targetBrightness = 128;
  float brightnessFactor = targetBrightness / avgBrightness;

  for (int i = 0; i < img.pixels.length; i++) {
    color c = img.pixels[i];
    float r = constrain(red(c) * brightnessFactor, 0, 255);
    float g = constrain(green(c) * brightnessFactor, 0, 255);
    float b = constrain(blue(c) * brightnessFactor, 0, 255);
    img.pixels[i] = color(r, g, b);
  }
  img.updatePixels();
}


// ----------------------------------------------------
//         Draw bar graph for recent readings
// ----------------------------------------------------

void drawGraph() {
  int graphTransparency = 100;
  int graphTop = height - graphHeight; // Start of graph
  int topSpacing = 15;

  // Calculate the displayed range manually from the ArrayList
  int displayedMax = Integer.MIN_VALUE; // Start with the smallest possible integer
  int displayedMin = Integer.MAX_VALUE; // Start with the largest possible integer
  
  for (int i = 0; i < recentReadings.size(); i++) {
    int reading = recentReadings.get(i);
    displayedMax = max(displayedMax, reading);
    displayedMin = min(displayedMin, reading);
  }

  int range = displayedMax - displayedMin == 0 ? 1 : displayedMax - displayedMin; // Avoid divide-by-zero error

  // Calculate bar width based on the size of the recentReadings
    float barWidth = (float) width / recentReadings.size();  // Use float for precise width

  // Set a translucent background for the graph area
  //fill(50, 50, 50, graphTransparency); // Dark gray with some transparency
  //rect(0, graphTop - topSpacing, width, graphHeight + topSpacing);

  for (int i = 0; i < recentReadings.size(); i++) {
    int reading = recentReadings.get(i);
  
    // Map the readings to the x-axis and bar height
    float xPos = i * barWidth;  // Use float for xPos
    int barHeight = 0;
    if (displayedMin != displayedMax) {
        barHeight = (int) map(reading, displayedMin, displayedMax, 0, graphHeight);
    }
    // Use semi-transparent green for bars
    fill(0, 255, 0, graphTransparency); // Green with alpha set to 150 (semi-transparent)
    rect(xPos, graphTop + graphHeight - barHeight, barWidth - 1, barHeight);  // Draw the bar
  
    // Draw the value on top of the bar
    fill(255); // White text
    textSize(8);
    textAlign(CENTER, BOTTOM);
    text(reading, xPos + barWidth / 2, graphTop + graphHeight - barHeight - 2);  // Display the value
  }
}


// ----------------------------------------------------
//       adaptive movement detected threshold 
// ----------------------------------------------------
// The procedure sorts recentReadings, calculates the median and interquartile 
// range (IQR), and sets saveThreshold to the median plus 1.5 times the IQR. 
// This adapts the threshold to current data, ignoring outliers.

void updateThreshold() {
  // Create a copy of recentReadings
  ArrayList<Integer> sortedReadings = new ArrayList<Integer>(recentReadings);

  // Sort the ArrayList using a simple sorting algorithm
  sortedReadings.sort((a, b) -> a - b);

  int n = sortedReadings.size();
  if (n == 0) return; // Avoid division by zero

  // Calculate median
  float median = (n % 2 == 0) ? 
    (sortedReadings.get(n/2 - 1) + sortedReadings.get(n/2)) / 2.0 : 
    sortedReadings.get(n/2);

  // Calculate Q1 and Q3 for IQR
  float q1 = sortedReadings.get(n/4);
  float q3 = sortedReadings.get(3*n/4);
  float iqr = q3 - q1;

  // Adjust saveThreshold based on median and IQR
  saveThreshold = int(median + 1.5f * iqr); // Example factor
}


// ----------------------------------------------------
//             delete older image files
// ----------------------------------------------------

void deleteOldFiles(String folderPath) {
  println(getCurrentDateTime() + "[camera] Deleting older image files (" + myParam + ")");
  File folder = new File(folderPath);

  if (!folder.exists() || !folder.isDirectory()) {
    println("[camera]" + myParam + " - Invalid folder path: " + folderPath);
    return;
  }

  // Get all files in the folder
  File[] files = folder.listFiles();

  if (files == null || files.length == 0) {
    println("[camera]" + myParam + " - No files to check in: " + folderPath);
    return;
  }

  // Calculate the cutoff date 
  long cuttofInMillis = imageAgeLimit * 24 * 60 * 60 * 1000; // 14 days in milliseconds
  long cutoffDate = System.currentTimeMillis() - cuttofInMillis;

  // Loop through each file
  for (File file : files) {
    if (file.isFile()) {
      // Check the last modified date
      long lastModified = file.lastModified();

      // If the file is older than 2 weeks, delete it
      if (lastModified < cutoffDate) {
        if (file.delete()) {
          println("[camera]" + myParam + " - Deleted: " + file.getName());
        } else {
          println("[camera]" + myParam + " - Failed to delete: " + file.getName());
        }
      } else {
        //println("[camera]" + myParam +" - Retained: " + file.getName());
      }
    }
  }
}


// ----------------------------------------------------
//               return the date and time
// ----------------------------------------------------

String getCurrentDateTime() {
  // Get the current date and time
  int year = year();
  int month = month();
  int day = day();
  int hour = hour();
  int minute = minute();
  int second = second();

  // Construct the date and time string
  String dateTime = nf(day, 2) + "/" + nf(month, 2) + "/" + year + " " + nf(hour, 2) + ":" + nf(minute, 2) + ":" + nf(second, 2);

  return dateTime;
}


// ----------------------------------------------------
//               send a UDP broadcast
// ----------------------------------------------------

void sendUDPmessage(String message) {
    if (UDPenabled == false) return;
    long currentTime = millis();
    if (currentTime - lastUDPtrigger >= (minTriggerTime * 1000) || lastUDPtrigger == 0) {
          lastUDPtrigger = currentTime;
          byte[] buffer = message.getBytes();
          DatagramPacket packet = new DatagramPacket(buffer, buffer.length, broadcastAddress, port);
          try {
            socket.send(packet);
            println("Message sent!");
          } catch (IOException e) {
            e.printStackTrace();
          }  
     }
}


// ----------------------------------------------------
//                    Make a sound
// ----------------------------------------------------

void makeSound() {
    long currentTime = millis();
    if (currentTime - lastSoundtrigger >= (minTriggerTime * 1000) || lastSoundtrigger == 0) {
        beep.rewind();
        beep.play(); 
        lastSoundtrigger = currentTime;
    }
}
  
  
// ----------------------------------------------------
// end
