
// PROCESSING file to read a raw rgb data file and display it

//                       Alanesq - 13Nov20

// ----------------------------------------------------------


// image file (csv)
  String fileName = "q.rgb";

// Image resolution
  int resX = 640;
  int resY = 480;


byte[] rgbFile; 
int xPos = 0;
int yPos = 0;


//   ----------------------------------------------------------------------------------------


void setup() {
  
  // open csv file
    rgbFile = loadBytes(fileName);
    print(fileName + " file size = ");
    println(rgbFile.length);
  
  size(640,480);
  background(0);   
  noLoop();
  stroke(255);
  
}  // setup


//   ----------------------------------------------------------------------------------------


void draw() {
  
  for(int i = 0; i < (rgbFile.length - 2); i+=3) {
          // increment image position
            xPos = xPos + 1;
            if (xPos == resX) {
               xPos = 0;
               yPos = yPos  + 1;
            }
            stroke(rgbFile[i+2] & 0xff,rgbFile[i+1] & 0xff,rgbFile[i+0] & 0xff);
            point(resX-xPos,yPos);
    }
 
 println("Finished");

}   // draw


//   ----------------------------------------------------------------------------------------

  
