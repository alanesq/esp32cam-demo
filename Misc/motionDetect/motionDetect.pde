/*
 ----------------------------------------------------

 ESP32cam motion detection using Processing - 02Jan25

        uses libraries OpenCV and Minim

 ----------------------------------------------------

 Notes: 
     Convert jpg files to amovie:  convert -delay 10 *.jpg output.mp4

*/

// ----------------------------------------------------
//                       Settings
// ----------------------------------------------------

  String myParam = "ESPCam";                                          // camera title
  String imgUrl = "http://192.168.1.2/jpg" + "?image.jpg";            // url of camera
  String imageFolder = sketchPath() + "/images/";                     // folder to store images
        
  boolean soundEnabled = false;      // if sound when motion detected
  int triggerLevel = 20;             // movement trigger level above baseline level
  int enableAdaptiveTriggerLevel = 1;// if trigger level adapts to previous movement levels (1 or 0)
  int windowSize = 50;               // Sliding window size for average calculation
  long imageAgeLimit = 7;            // days to keep stored images
  int resizeX = 160;                 // size of image created to motion detect
  int resizeY = 120; 
  int changeThreshold = 25;          // Pixel intensity change threshold
  int delay = 800;                   // Delay in milliseconds between draw calls (ms)
  float alpha = 0.1;                 // Smoothing factor (0.0 < alpha <= 1.0)
  int graphHeight = 80;              // Height of the graph area

  // A 4x4 movement detection mask (true = ignore area) - masked area is shown as yellow
      boolean[][] mask = {
        { false, false, false, false },        // Top row
        { false, false, false, false },
        { false, false, false, false },
        { false, false, false, false }         // Bottom row
      };


// ----------------------------------------------------


// required to delete old image files
  import java.io.File;
  import java.util.Date;
  
  
// sound toggle button
  int buttonX = 5;      // X position of the button
  int buttonY = 55;     // Y position of the button
  int buttonWidth = 75;
  int buttonHeight = 25;


PImage currentImg, previousImg, motionOverlay;
int lastRun = 0;                 // time the daily deletion of older images was last performed
int saveThreshold = 1000000 - triggerLevel;  // self adaptive movement trigger level
int refreshRate = 4;             // frame rate
int lastDrawTime = 0;            // Stores the last time draw() was called
int movementLevel = 0;           // Accumulated movement level
ArrayList<Integer> recentReadings = new ArrayList<Integer>(); // Store past readings
int maxReading = 0;              // Maximum reading for bar graph normalization
String lastDownloadTime = "";    // Store last image fetch time

// sound
import ddf.minim.*;
Minim minim;
AudioPlayer beep;


// ----------------------------------------------------
//                       -setup
// ----------------------------------------------------

void setup() {
  frameRate(refreshRate);
  size(640, 480);
  surface.setResizable(true);

  // audio
    minim = new Minim(this);
    beep = minim.loadFile("beep.wav");  
    
  surface.setTitle("Motion: " + myParam);
  
  // request image from camera
    currentImg = requestImg();   // load first image       
    normalizeBrightness(currentImg);
    if (currentImg != null) previousImg = currentImg.get();
    
  if (soundEnabled == true) {
    beep.rewind();
    beep.play();  
  }    
  
  if (enableAdaptiveTriggerLevel == 0) saveThreshold = 0;        // if adaptive trigger is disabled
  
  println(getCurrentDateTime() + " camera '" + myParam + "' starting");
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
        normalizeBrightness(currentImg);    // compensate for changes in brightness
        
      if (previousImg != null && currentImg != null) {
      
        MovementResult diff = compareImages(previousImg, currentImg, mask);
        movementLevel = diff.movementLevel;

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
            text("Movement:" + movementLevel + "  Trigger:" + (saveThreshold + triggerLevel), 10, 40);
            text(myParam + ": " + lastDownloadTime, 10, 20);
            
        // if movement threshold exceeded
          if (enableAdaptiveTriggerLevel == 1) updateThreshold();   // adapt threshold level
          if (movementLevel > saveThreshold + triggerLevel) {
            println(getCurrentDateTime() + " Movement detected (" + myParam +")");
            //currentImg.save(imageFolder + lastDownloadTime + ".jpg"); 
            save(imageFolder + lastDownloadTime + ".jpg"); 
            if (soundEnabled == true) {
                beep.rewind();
                beep.play();  
            }
          }
           
        // Draw movement graph
          tint(255, 96); 
          drawGraph();          
          
        // display movement detection image on top of camera image 
          tint(255, 96); 
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
//           sound on/off toggle button
// ----------------------------------------------------

void togButton() {
    // Draw the toggle button
        if (soundEnabled) {
          fill(0, 255, 0); // Green when ON
        } else {
          fill(255, 0, 0); // Red when OFF
        }
        tint(255, 64); 
        rect(buttonX, buttonY, buttonWidth, buttonHeight);  
    // Draw button label
        fill(0, 0, 255);   // blue
        tint(255, 64); 
        textSize(12);
        textAlign(CENTER, CENTER);
        text(soundEnabled ? "Sound ON" : "Sound OFF", buttonX + buttonWidth / 2, buttonY + buttonHeight / 2);
}


// ----------------------------------------------------
//                  save image button
// ----------------------------------------------------

void saveButton() {
    // Draw the save button
        fill(0, 255, 0); // Green when ON
        tint(255, 64); 
        rect(buttonX + 5 + buttonWidth, buttonY, buttonWidth, buttonHeight);  
    // Draw button label
        fill(0, 0, 255);   // blue
        tint(255, 64); 
        textSize(12);
        textAlign(CENTER, CENTER);
        text("Save", 5 + buttonWidth + buttonWidth / 2 , buttonY + buttonHeight / 2);
}
 
void mousePressed() {    
  if (mouseX > buttonX && mouseX < buttonX + buttonWidth && mouseY > buttonY && mouseY < buttonY + buttonHeight) {
    soundEnabled = !soundEnabled; // Toggle the flag
  }
  if (mouseX > buttonX + buttonWidth + 5 && mouseX < buttonX + (2 * buttonWidth) + 5 && mouseY > buttonY && mouseY < buttonY + buttonHeight) {
    save(imageFolder + lastDownloadTime + ".jpg");    // save image
  }  
}  


// ----------------------------------------------------
//                load image from camera
// ----------------------------------------------------

PImage requestImg() {
    int attemptsToTry = 3;
    int retries = attemptsToTry;

    while (retries-- > 0) {
        PImage img = null; // Always start fresh
        try {
            img = requestImage(imgUrl); // Attempt to load the image
            int startTime = millis();
            while (img != null && img.width < 1) { // Wait for a valid image
                if (millis() - startTime >= 5000) { // Timeout
                    println(getCurrentDateTime() + " Image load timed out (" + myParam + ")");
                    img = null; // Clear invalid image
                    break;
                }
                delay(50);
            }
            if (img != null && img.width > 1 && img.height > 1) {        // Successfully loaded
                lastDownloadTime = day() + "-" + month() + "-" + year() + "--" + hour() + ":" + nf(minute(), 2) + ":" + nf(second(), 2);
                if (retries != attemptsToTry - 1) println(getCurrentDateTime() + " camera '" + myParam + "' image captured ok");
                return img;
            } else {
                println(getCurrentDateTime() + " camera '" + myParam + "' image capture failed");
            }
        } catch (Exception e) {
            println(getCurrentDateTime() + " camera '" + myParam + "' Exception during image request: " + e.getMessage());
        }
        println(myParam + " Retrying... (" + retries + " attempts left)");
        delay(800); // Allow cooldown between retries
    }
    println(getCurrentDateTime() + myParam + " - Failed to fetch image after multiple attempts (" + myParam + ")");
    return null;
}


// ----------------------------------------------------
//                  compare two images
// ----------------------------------------------------
// a mask in the form of a 4x4 grid can be supplied (true = ignore area)

MovementResult compareImages(PImage img1, PImage img2, boolean[][] mask) {
  // Resize images for faster computation
  PImage smallImg1 = img1.get(); // Make a copy of img1
  PImage smallImg2 = img2.get(); // Make a copy of img2
  smallImg1.resize(resizeX, resizeY); // Resize modifies the image in place
  smallImg2.resize(resizeX, resizeY); // Resize modifies the image in place

  smallImg1.loadPixels();
  smallImg2.loadPixels();

  motionOverlay = createImage(smallImg1.width, smallImg1.height, RGB);
  motionOverlay.loadPixels();

  int movementLevel = 0;

  int gridWidth = smallImg1.width / 4;
  int gridHeight = smallImg1.height / 4;

  for (int y = 0; y < smallImg1.height; y++) {
    for (int x = 0; x < smallImg1.width; x++) {
      int i = x + y * smallImg1.width;

      // Determine which grid cell the pixel belongs to
      int gridX = x / gridWidth;
      int gridY = y / gridHeight;

      // Check if the mask excludes this cell
      if (mask != null && mask[gridY][gridX]) {
        //motionOverlay.pixels[i] = smallImg1.pixels[i];   // Keep original pixel
        motionOverlay.pixels[i] = color(255, 255, 0);      // yellow
        continue;
      }

      float diff = brightness(smallImg1.pixels[i]) - brightness(smallImg2.pixels[i]);
      if (abs(diff) > changeThreshold) {
        movementLevel++;
        // Highlight motion in green
        motionOverlay.pixels[i] = color(0, 255, 0);
      } else {
        // Keep original pixel
        motionOverlay.pixels[i] = smallImg1.pixels[i];
      }
    }
  }

  motionOverlay.updatePixels();

  // Display the image with motion highlighted
  image(motionOverlay, 0, 0, width, height);

  return new MovementResult(movementLevel);
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
  println(getCurrentDateTime() + " Deleting older image files (" + myParam + ")");
  File folder = new File(folderPath);

  if (!folder.exists() || !folder.isDirectory()) {
    println(myParam + " - Invalid folder path: " + folderPath);
    return;
  }

  // Get all files in the folder
  File[] files = folder.listFiles();

  if (files == null || files.length == 0) {
    println(myParam + " - No files to check in: " + folderPath);
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
          println(myParam + " - Deleted: " + file.getName());
        } else {
          println(myParam + " - Failed to delete: " + file.getName());
        }
      } else {
        //println(myParam +" - Retained: " + file.getName());
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
  String dateTime = nf(month, 2) + "/" + nf(day, 2) + "/" + year + " " + nf(hour, 2) + ":" + nf(minute, 2) + ":" + nf(second, 2);

  return dateTime;
}


// ----------------------------------------------------
//               used for comparing images
// ----------------------------------------------------
class MovementResult {
  int movementLevel;
  MovementResult(int movementLevel) {
    this.movementLevel = movementLevel;
  }
}


// ----------------------------------------------------
// end
