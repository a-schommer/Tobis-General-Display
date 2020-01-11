/*

Tobis General Display
by Arnold Schommer, Tobias Kuch

bitmap.h - definition of a (Windows) Bitmap file header structure and prototype(!) of a function to read that from a file

based on: the sketch on the German website https://www.az-delivery.de/blogs/azdelivery-blog-fur-arduino-und-raspberry-pi/captive-portal-blog-teil-4-bmp-dateienanzeige-auf-8x8-matrix-display
by Tobias Kuch

*/

#ifndef BITMAP_H
#define BITMAP_H

struct BMPHeader // BitMapStucture
  {
    uint32_t fileSize;
    uint32_t creatorBytes;
    uint32_t imageOffset;   // start of image data, "image offset"
    uint32_t headerSize;
    uint32_t width;
    uint32_t height;
    uint16_t planes;
    uint16_t depth; // bits per pixel
    uint32_t format;
  };

BMPHeader ReadBitmapSpecs(String filename);

#endif BITMAP_H
