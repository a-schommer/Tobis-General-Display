/*

Tobis General Display

JPEG_functions.ino

This file contains support functions to render the Jpeg images.

Created by Bodmer 15th Jan 2017
  
adapted by Arnold Schommer 2019

u8g2 variant

==================================================================================*/

#include <U8g2lib.h>        // https://github.com/olikraus/u8g2
#include "gfxlayer.h"
#include <JPEGDecoder.h>    // https://github.com/Bodmer/JPEGDecoder

// extern void drawRGBBitmap(uint16_t x, uint16_t y, uint16_t *pImg, uint16_t win_w, uint16_t win_h);

// Return the minimum of two values a and b
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

//====================================================================================
//   Opens the image file and prime the Jpeg decoder
//====================================================================================
void drawJpeg_SPIFFS(const char *filename) {

  Serial.println("===========================");
  Serial.print("Drawing file: "); Serial.println(filename);
  Serial.println("===========================");

  // Open the named file (the Jpeg decoder library will close it after rendering image)
  fs::File jpegFile = SPIFFS.open( filename, "r");    // File handle reference for SPIFFS
  //  File jpegFile = SD.open( filename, FILE_READ);  // or, file handle reference for SD library
 
  if ( !jpegFile ) {
    Serial.print("ERROR: File \""); Serial.print(filename); Serial.println ("\" not found!");
    return;
  }

  // Use one of the three following methods to initialise the decoder:
  //boolean decoded = JpegDec.decodeSdFile(jpegFile); // or pass the SD file handle to the decoder,
  boolean decoded = JpegDec.decodeFsFile(filename);  // or pass the filename (leading / distinguishes SPIFFS files)
                                   // Note: the filename can be a String or character array type
  if (decoded) {
    //uint16_t xpos = (gfx_getScreenWidth()-JpegDec.width)/2,
    //         ypos = (gfx_getScreenHeight()-JpegDec.height)/2;
             
    // prepare a framebuffer
    if(!prepare_framebuffer(JpegDec.width, JpegDec.height)) return;
  
    // print information about the image to the serial port
    jpegInfo();

    // render the image into a framebuffer without offset
    jpegRender(0, 0);   //jpegRender(xpos, ypos);

    // "copy"/"convert" framebuffer to screen, applying Floyd-Steinberg-dithering
    framebuffer_to_display(JpegDec.width, JpegDec.height);
  }
  else {
    Serial.println("Jpeg file format not supported!");
  }
}

//====================================================================================
//   Decode and render the Jpeg image onto the screen
//====================================================================================
void jpegRender(int xpos, int ypos) {

  // retrieve infomration about the image
  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth,
           mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width,
           max_y = JpegDec.height;

  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = minimum(mcu_w, max_x % mcu_w),
           min_h = minimum(mcu_h, max_y % mcu_h);

  // the current image block size
  uint32_t win_w, win_h;

  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();

  gfx_clearScreen();    // clear previous image

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // read each MCU block until there are no more
  while ( JpegDec.read()) {

    // save a pointer to the image block
    pImg = JpegDec.pImage;

    // calculate where the image block should be drawn on the screen
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right edge
    win_w = (mcu_x + mcu_w <= max_x) ? mcu_w:min_w;

    // check if the image block size needs to be changed for the bottom edge
    win_h = (mcu_y + mcu_h <= max_y) ? mcu_h:min_h;

    // copy pixels into a contiguous block
    if (win_w != mcu_w)
      for (int h = 1; h < win_h-1; h++)
        memcpy(pImg + h * win_w, pImg + (h + 1) * mcu_w, win_w << 1);

    // draw image MCU block only if it will fit on the screen
    if ( ( mcu_x + win_w) <= gfx_getScreenWidth() && ( mcu_y + win_h) <= gfx_getScreenHeight())
        drawRGBTile(mcu_x, mcu_y, pImg, win_w, win_h);
    else if ( ( mcu_y + win_h) >= gfx_getScreenHeight()) 
        JpegDec.abort();
  }
  gfx_flushBuffer();

  // calculate how long it took to draw the image
  drawTime = millis() - drawTime; // Calculate the time it took

  // print the results to the serial port
  Serial.print  ("Total render time was    : "); Serial.print(drawTime); Serial.println(" ms");
  Serial.println("=====================================");

}

//====================================================================================
//   Print information decoded from the Jpeg image
//====================================================================================
void jpegInfo() {

  Serial.println("===============");
  Serial.println("JPEG image info");
  Serial.println("===============");
  Serial.print  ("Width      :"); Serial.println(JpegDec.width);
  Serial.print  ("Height     :"); Serial.println(JpegDec.height);
  Serial.print  ("Components :"); Serial.println(JpegDec.comps);
  Serial.print  ("MCU / row  :"); Serial.println(JpegDec.MCUSPerRow);
  Serial.print  ("MCU / col  :"); Serial.println(JpegDec.MCUSPerCol);
  Serial.print  ("Scan type  :"); Serial.println(JpegDec.scanType);
  Serial.print  ("MCU width  :"); Serial.println(JpegDec.MCUWidth);
  Serial.print  ("MCU height :"); Serial.println(JpegDec.MCUHeight);
  Serial.println("===============");
  Serial.println("");
}
//====================================================================================
