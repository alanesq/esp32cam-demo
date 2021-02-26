/* 

       PROCESSING file to read a raw rgb data file and display it
       
       This would be a file created from the command:   client.write(rgb, ARRAY_LENGTH);

                             Alanesq - 13Nov20

       for info on coding with Processing see: https://thecodingtrain.com/


// ---------------------------------------------------------- */


// General Settings

// image file to read (raw RGB)
  String fileName = "q.rgb";

// Image resolution
  int resX = 640;
  int resY = 480;


// ----------------------------------------------------------


// Misc variables 

  byte[] rgbFile;            // store for the file contents


// ----------------------------------------------------------


void setup() {
  
  // open the RGB file
    rgbFile = loadBytes(fileName);
    print(fileName + " file size = ");
    println(rgbFile.length);
    print("Drawing image");
  
  size(640,480);        // display screen size
  background(0);        // background colour of screen (0 = black)
  noLoop();             // do not keep looping the draw procedure
  
}  // setup


// ----------------------------------------------------------


void draw() {
  
  int xPos;
  int yPos;
    
  // work through the RGB file plotting each individual colour pixel on the screen
  for(int i = 0; i < (rgbFile.length - 2); i+=3) {
      // Calculate x and y location in image  
        xPos = (i / 3) % resX;
        yPos = floor( (i / 3) / resX );
      stroke(rgbFile[i+2] & 0xff,rgbFile[i+1] & 0xff,rgbFile[i+0] & 0xff);
      point(xPos,yPos);
      if ( (i % 5000) == 0 ) print(".");       // show progress
   }

 println("Finished");

}   // draw


// ----------------------------------------------------------
// end
