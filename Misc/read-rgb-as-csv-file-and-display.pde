
// read a csv file containing RGB data and display the image

// CSV file contains one line for pixel in the format "rrr,ggg,bbb"    
// e.g.     12,34,56
//          34,128,13
//          67,77,89

// -------------------------------------------------------

// image file name (csv)
  String fileName = "/home/alan/temp/q.csv";

// Image resolution
  int resX = 320;
  int resY = 240;


String[] lines; 
int xPos = 0;
int yPos = 0;


void setup() {
  // open csv file
    lines = loadStrings(fileName);
    print(lines.length);
    println(" Lines in file");
  
  size(640,480);
  background(0);   
  noLoop();
  stroke(255);
}  // setup


void draw() {
  for(int i = 0; i < lines.length; i++) {
        String[] a = split(lines[i], ',');  
        if (a.length == 3) {                         // if expected number of variables received
          // increment image position
            xPos = xPos + 1;
            if (xPos == resX) {
               xPos = 0;
               yPos = yPos  + 1;
            }
            stroke(float(a[0]),float(a[1]),float(a[2]));
            point(resX-xPos,resY-yPos);
        }       
  }
  
  println("Finished");

}   // draw
  
