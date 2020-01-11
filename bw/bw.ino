/*

Tobis General Display
by Arnold Schommer, Tobias Kuch

bw.ino - main sketch, u8g2 variant

based on: the sketch on the German website https://www.az-delivery.de/blogs/azdelivery-blog-fur-arduino-und-raspberry-pi/captive-portal-blog-teil-4-bmp-dateienanzeige-auf-8x8-matrix-display
by Tobias Kuch

changes/extensions:

- display: black&white only SSD1306 OLED (or else, using u8g2lib) with higher resolution (128x64 e.g.) instead of 8x8 LED matrix with colour capability
- smaller images are displayed centered, not in the top left corner
- slideshow function: the MCU can display all suitable images cyclically
- Floyd-Steinberg-Dithering for multicolor images (makes sense on b&w or grayscale display)
- check, which of the colors in a 1-bit image file is lighter
- option (menu item) to display the IP address - instead of an image
- (limited) JPEG capability using the JPEGDecoder lib from Bodmer
- taking some of the configuration out of this main file to the newly created config.h
- compilable for ESP32 *and* ESP8266
- display of IP adddress, SSID & WiFi password can be configured in the EEPROM-data

*/

#include "pre-config.h"
#include "config.h"
#include <string.h>
#include "esplayer.h"
#include <DNSServer.h>
#include "settings.h"
#include <U8g2lib.h>        // https://github.com/olikraus/u8g2
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
#include "bitmap.h"
#include <JPEGDecoder.h>    // https://github.com/Bodmer/JPEGDecoder
#include "network.h"

// u8g2 object:
U8G2_CONSTRUCTION;
#include "gfxlayer.h"       // << this unfortunately requires the ucg/u8g2 object to be declared before

bool slideshow_is_running = false;
unsigned long slideshow_last_switch = 0;
int slideshow_current_index = 0;        // next image to display

String temp ="";

void setup(void)
{
  esp_guru_meditation_error_remediation();
  bool ConnectSuccess = false;
  bool CreateSoftAPSucc  = false;
  bool CInitFSSystem  = false;
  bool CInitHTTPServer  = false;
  byte len;
  Serial.begin(BAUDS);
  while (!Serial) // wait for serial port to connect. Needed for native USB
    delay(1);
  Serial.println("serial interface initialized at "+String(BAUDS)+" baud");
  gfx_init();
#ifdef SPECIAL_INITIALIZATION
  SPECIAL_INITIALIZATION();
#endif
  WiFi.setAutoReconnect (false);
  WiFi.persistent(false);
  WiFi.disconnect();
  esp_wifi_set_hostname(ESPHostname); // Set the DHCP hostname assigned to ESP station.
  if (loadSettings())   // Load settings, mainly WiFi
  {
     Serial.println(F("Valid settings data found."));
     MySettings.WiFiAPSTAName[strlen(MySettings.WiFiAPSTAName)+1] = '\0';
     MySettings.WiFiPwd[strlen(MySettings.WiFiPwd)+1] = '\0';
     if (SETTINGS_IS_AP_MODE)
      {
        Serial.println(F("Access Point Mode selected."));
        CreateSoftAPSucc = CreateWifiSoftAP();
      } else
      {
        Serial.println(F("Station Mode selected."));
        len = ConnectWifiAP();
        if ( len == 3 ) { ConnectSuccess = true; } else { ConnectSuccess = false; }
      }
  } else
  { //set default settings - create AP
     Serial.println(F("NO valid settings data found."));
     SetDefaultSettings();
     CreateSoftAPSucc = CreateWifiSoftAP();
     saveSettings();
     // Blink
     delay(500);
  }
  // initialize filesystem
  CInitFSSystem = InitializeFileSystem();
  if (!(CInitFSSystem)) Serial.println(F("file system not initialized !"));
  if (ConnectSuccess || CreateSoftAPSucc)
    {
      //Serial.print (F("IP Address: "));
      //if (CreateSoftAPSucc) { Serial.println(WiFi.softAPIP());}
      //if (ConnectSuccess) { Serial.println(WiFi.localIP());}
    }
    else
    {
      Serial.setDebugOutput(true); //Debug Output for WiFi on Serial Interface.
      Serial.println(F("Error: Cannot connect to WiFi. Set DEFAULT WiFi configuration."));
      SetDefaultWiFiSettings();
      CreateSoftAPSucc = CreateWifiSoftAP();
      // saveSettings();
    }
    InitializeHTTPServer();

    doShowWifi(false);

    if(SETTINGS_IS_SLIDESHOW_AUTORUN)
    {
        slideshow_is_running = true;
        scan_images_for_slideshow();
        slideshow_last_switch = millis();
    }
}

boolean InitializeFileSystem() {
  bool initok = false;
  initok = SPIFFS.begin();
  delay(200);
  if (! initok)
  {
    Serial.println(F("Format SPIFFS"));
    SPIFFS.format();
    initok = SPIFFS.begin();
  }
  return initok;
}

uint16_t read16(File f)
{
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint16_t readrgbsum(File f)
{   // read a triple rgb plus a fourth dummy and just return the sum:
    return (uint16_t) f.read() + (uint16_t) f.read() + (uint16_t) f.read() + 0*f.read();
}

uint32_t read32(File f)
{
  // BMP data is stored little-endian, same as Arduino.
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

BMPHeader ReadBitmapSpecs(String filename)
{
  File file;
  BMPHeader BMPData;
  file = SPIFFS.open(filename, "r");
  if (!file)
  {
    file.close();
    return BMPData;
  }
  // Parse BMP header
  if (read16(file) == 0x4D42) // BMP signature
  {
    BMPData.fileSize = read32(file);
    BMPData.creatorBytes = read32(file);
    BMPData.imageOffset = read32(file); // Start of image data
    BMPData.headerSize = read32(file);
    BMPData.width  = read32(file);
    BMPData.height = read32(file);
    BMPData.planes = read16(file);
    BMPData.depth = read16(file); // bits per pixel
    BMPData.format = read32(file);
  }
  file.close();
  return BMPData;
}

//#############################################################################
// JPEG support framework
// Bodmers JPEG lib is optimized for low memory usage, which makes sense for
// Arduino but has the disadvantage that it draws the image tile by tile -
// making Floyd-Steinberg-dithering (almost) impossible.
// I add another "Framebuffer" to change the render process:
// first, Bodmers jpeg lib writes to a framebuffer via repeated drawRGBBitmap()
// then the whole framebuffer is converted to a b/w image, doing Floyd-
// Steinberg-dithering on the fly.
// in fact, the framebuffer does not hold the colors as given by the jpeg
// output but int8_t's of the added luminances (i.e. 0...3*255 scaled down to
// 0..127).
// (scaling fown from int16_t to int8_t is not really necessary concerning
//  memory consumption, but i see no relevant quality drawback on the way to
//  1bit resullts and shrinking the buffer should gain performance by better
//  caching effects)
// there are three (main) procedures:
// prepare_framebuffer()
//          allocates(*) a framebuffer and clears it
//          to save performance, the buffer is only allocated once and never
//          returned - with the backdraw it is allocated for full display size,
//          not just as required for the current image.
// drawRGBTile()
//          "paints" a rectangle given as an RGB565-array to a certain
//          location within the framebuffer
// framebuffer_to_display()
//          "paints" the framebuffer content to the SSD1306, applying
//          Floyd-Steinberg-dithering

// define a "scaling" factor for Floyd-Steinberg-dithering (must be <128! )
#define FS_SCALE_MAX    100
/* problem: if e.g. the colours range from 0-127, intermediate values (with
    error coefficients) can easily exceed 127 (by an amount i do not know).
    But if i use int8_t as datatype, this causes "overflows" like
    127+2 "=" -127.
    To prevent this, i use some "safety reserve"
    Further, to make the threshold more symmetric, this value should
    preferrably be even.
*/

int8_t *fb = NULL;
// used like fb[gfx_getScreenHeight()][gfx_getScreenWidth()]
// reason for this strange (logic) organization:
// i want to keep adjacent x-values adjacent, not adjacent y-values as
// the dithering walks throuh it line by line, columns iterated in "the inner" loop

bool prepare_framebuffer(const uint16_t width, const uint16_t height)
// width & height: concerning the image, not the framebuffer
{
    if(!fb)
    {   // allocate the framebuffer for full display size
        fb = (int8_t *)malloc(gfx_getScreenWidth()*gfx_getScreenHeight()*sizeof(*fb));
        if(!fb)
        {
            Serial.println("can't alloc framebuffer, "+String(gfx_getScreenWidth()*gfx_getScreenHeight()*sizeof(*fb))+" bytes unavailable");
            return false;
        }
    }
    // clear the relevant number of rows, all columns (reducing the columns might save some writes but require a loop...)
    memset((void *)fb, 0, height * gfx_getScreenWidth() * sizeof(*fb));
    return true;
}

//#############################################################################
// draw a tile already loaded to a small memory buffer - called/required by jpegRender()
// converting from RGB565 to grayscale, 0..FS_SCALE_MAX (int8_t)
// CAUTION: will crash, if prepare_framebuffer() is not yet called (successfully) !
void drawRGBTile(uint16_t x, uint16_t y, uint16_t *pImg, uint16_t width, uint16_t height)
{
    // Serial.println("drawRGBTile("+String(x)+", "+String(y)+", *pImg, "+String(width)+", "+String(height)+")");

    while(height > 0)
    {
        uint16_t cx;

        if(y >= gfx_getScreenHeight())  return; // abort if screen is left

        for(cx=0; cx<width; ++cx)
        {
            if(x+cx < gfx_getScreenWidth())
            {   // sum up the rgb parts of the RGB565-value at *pImg to one value:
                int16_t brightness = ((*pImg >> 8) & 0xf8) +  // r
                                     ((*pImg >> 3) & 0xfc) +  // g
                                     ((*pImg << 3) & 0xf8);   // b
                // save the "result", mapped to 0..FS_SCALE_MAX
                fb[x+cx+y*gfx_getScreenWidth()] = map(brightness, 0,0xf8+0xfc+0xf8, 0,FS_SCALE_MAX);
                    // 0xf8+0xfc+0xf8: you might expect the maximum of the brightness from r+g+b (8bit) to be 255,
                    // but the values are RGB565, not RGB888, and RGB565 allows maxima of 0xf8/0xfc/0xf8.
            }
            ++pImg; // advance in the buffer
        }
        // advance to next row
        ++y; --height;
    }
}

// "paint" the framebuffer content to the SSD1306, applying
// Floyd-Steinberg-dithering
void framebuffer_to_display(uint16_t width, uint16_t height)
{
    int8_t *fsd_error_buffer,               // buffer for the error coefficients of Floyd-Steinberg-dithering, (approximately) two lines only!
           *fsd_this_line, *fsd_next_line;  // these switch between first and second "half"/line of fsd_error_buffer
#define FSD_LINESIZE    ((width)+2)
#define FSD_INDEX(x)    ((x)+1)
    uint16_t offset_x = (gfx_getScreenWidth() -width )/2,
             offset_y = (gfx_getScreenHeight()-height)/2;

    // prepare a buffer of two lines plus(!) two "pixels" each for error coefficients of Floyd-Steinberg-dithering
    fsd_error_buffer = (int8_t *) calloc(2*FSD_LINESIZE, sizeof(*fsd_error_buffer));
    if(!fsd_error_buffer)
    {
        Serial.println("can't alloc buffer(s) for Floyd-Steinberg-dithering, "+String(2*FSD_LINESIZE*sizeof(*fsd_error_buffer))+" bytes unavailable; aborting drawing");
        return;
    }
    // Serial.println(String(2*FSD_LINESIZE*sizeof(*fsd_error_buffer))+" bytes of buffer(s) for Floyd-Steinberg-dithering allocated");
    fsd_this_line = fsd_error_buffer;
    fsd_next_line = fsd_error_buffer+FSD_LINESIZE;

    gfx_clearScreen();
    for (uint16_t row = 0; row < height; row++) // for each line
    {
        // swap lines concerning Floyd-Steinberg buffer; clear next line
        if(row>0)
        {
            int8_t *h;
            h=fsd_this_line;
            fsd_this_line=fsd_next_line;
            fsd_next_line=h;
            // clear next line error buffer
            memset(fsd_next_line, 0, FSD_LINESIZE*sizeof(*fsd_error_buffer));
        }

        for (uint16_t col = 0; col < width; col++) // for each pixel
        {
            int8_t setpoint = fb[col+row*gfx_getScreenWidth()] + fsd_this_line[FSD_INDEX(col)],
                   real, qerror;
            if(setpoint > FS_SCALE_MAX/2)
            {
                gfx_setPixel(col+offset_x, row+offset_y);
                real = FS_SCALE_MAX;
            }
            else
            {
                real = 0;
            }
            qerror = setpoint - real;
            // propagate quantization error accodring to Floyd-Steinberg:
            fsd_this_line[FSD_INDEX(col+1)] += qerror*7/16;
            fsd_next_line[FSD_INDEX(col-1)] += qerror*3/16;
            fsd_next_line[FSD_INDEX(col  )] += qerror*5/16;
            fsd_next_line[FSD_INDEX(col+1)] += qerror  /16;
        } // end pixel
      } // end line
    free(fsd_error_buffer);
    gfx_flushBuffer(); // Show results :)
}

// end of JPEG support framework
//#############################################################################

#define SD_BUFFER_PIXELS 20

void drawBitmap_SPIFFS(const char *filename)
{
  File file;
  uint8_t buffer[3 * SD_BUFFER_PIXELS]; // pixel buffer, size for r,g,b
  bool valid = false; // valid format to be handled
  bool flip = true; // bitmap is stored bottom-to-top
  uint32_t pos = 0;

  file = SPIFFS.open(filename, "r");
  if (!file)
  {
    Serial.print(F("Filesytem Error"));
    return;
  }
  // Parse BMP header
  if (read16(file) == 0x4D42) // BMP signature
  {
    uint32_t fileSize = read32(file);
    uint32_t creatorBytes = read32(file);
    uint32_t imageOffset = read32(file); // Start of image data
    uint32_t headerSize = read32(file);
    uint32_t width  = read32(file);
    uint32_t height = read32(file);
    uint16_t planes = read16(file);
    uint16_t depth = read16(file); // bits per pixel
    uint32_t format = read32(file); // compression format; 0=uncompressed
    if ((planes == 1) && (format == 0) &&   // uncompressed is handled
        ((depth == 1) || (depth == 24)))    // only 1 or 24bits color depth impplemented
    {
      int16_t *fsd_error_buffer,                // buffer for the error coefficients of Floyd-Steinberg-dithering, (approximately) two lines only!
              *fsd_this_line, *fsd_next_line;   // these switch between first and second "half"/line of fsd_error_buffer
      uint8_t inverter;
#define FSD_LINESIZE    ((width)+2)
#define FSD_INDEX(x)    ((x)+1)
      // reason for "+1": the dithering will always "postpone" a part of the error to the some pixels left and right of the current one -
      // i do not deal with the edges, i just make them "part of what is buffered" to prevent buffer overflows etc. but never read from them.

      valid = true;
      Serial.print(F("File name: "));
      Serial.println(filename);
      Serial.print(F("File size: "));
      Serial.println(fileSize);
      Serial.print(F("Image Offset: "));
      Serial.println(imageOffset);
      Serial.print(F("Header size: "));
      Serial.println(headerSize);
      Serial.print(F("Bit Depth: "));
      Serial.println(depth);
      Serial.print(F("Image size: "));
      Serial.print(width);
      Serial.print('*');
      Serial.println(height);

      if(depth == 24)
      {   // prepare a buffer of two lines plus(!) two "pixels" each for error coefficients of Floyd-Steinberg-dithering
          fsd_error_buffer = (int16_t *) calloc(2*FSD_LINESIZE, sizeof(*fsd_error_buffer));
          if(!fsd_error_buffer)
          {
              Serial.println("can't alloc buffer(s) for Floyd-Steinberg-dithering, "+String(2*FSD_LINESIZE*sizeof(*fsd_error_buffer))+" bytes unavailable; aborting drawing of "+String(filename));
              file.close();
              return;
          }
          // Serial.println(String(2*FSD_LINESIZE*sizeof(*fsd_error_buffer))+" bytes of buffer(s) for Floyd-Steinberg-dithering allocated");
          fsd_this_line = fsd_error_buffer;
          fsd_next_line = fsd_error_buffer+FSD_LINESIZE;
      }
      else
      {   // depending on which palette color is lighter, this "becomes" white on the display:
          file.seek(5*sizeof(uint32_t),SeekCur);    // skip remainder of header, go to start of color table
          inverter = (readrgbsum(file) > readrgbsum(file)) ? ~0 : 0;
      }
      uint32_t rowSize = (width * depth / 8 + 7) & ~7;
      if (height < 0)
      {
        height = -height;
        flip = false;
      }
      gfx_clearScreen();
      uint16_t w = width,  offset_x = (gfx_getScreenWidth()-w)/2;
      uint16_t h = height, offset_y = (gfx_getScreenHeight()-h)/2;
      size_t buffidx = sizeof(buffer); // force buffer load
      for (uint16_t row = 0; row < h; row++) // for each line
      {
        pos = imageOffset +
              (flip ? ((height - 1 - row) * rowSize) :  // Bitmap is stored bottom-to-top order (normal BMP)
                      (row * rowSize) );                // Bitmap is stored top-to-bottom
        if (file.position() != pos)
        { // Need seek?
          file.seek(pos,SeekSet);   // if mode is SeekSet, position is set to offset bytes from the beginning.
                                    // if mode is SeekCur, current position is moved by offset bytes.
                                    // if mode is SeekEnd, position is set to offset bytes from the end of the
          buffidx = sizeof(buffer); // force buffer reload
        }
        // swap lines concerning Floyd-Steinberg buffer; clear next line
        if((depth == 24) && (row>0))
        {
            int16_t *h;
            h=fsd_this_line;
            fsd_this_line=fsd_next_line;
            fsd_next_line=h;
            // clear next line error buffer
            memset(fsd_next_line, 0, FSD_LINESIZE*sizeof(*fsd_error_buffer));
        }

        uint8_t bits;
        for (uint16_t col = 0; col < w; col++) // for each pixel
        {
          // Time to read more pixel data?
          if (buffidx >= sizeof(buffer))
          {
            file.read(buffer, sizeof(buffer));
            buffidx = 0; // Set index to beginning
          }
          switch (depth)
          {
            case 1: // one bit per pixel b/w format
              {
                if (0 == col % 8)
                    bits = buffer[buffidx++] ^ inverter;
                if(bits & 0x80)
                    gfx_setPixel(col+offset_x, row+offset_y);
                bits <<= 1;
              }
              break;
            case 24: // standard BMP format
              {
                int16_t b = (int16_t)buffer[buffidx++],
                        g = (int16_t)buffer[buffidx++],
                        r = (int16_t)buffer[buffidx++],
                        setpoint = r+b+g+fsd_this_line[FSD_INDEX(col)],
                        real, qerror;
                if(setpoint > 255*3/2)
                {
                    gfx_setPixel(col+offset_x, row+offset_y);
                    real = 255*3;
                }
                else
                {
                    real = 0;
                }
                qerror = setpoint - real;
                // propagate quantization error accodring to Floyd-Steinberg:
                fsd_this_line[FSD_INDEX(col+1)] += qerror*7/16;
                fsd_next_line[FSD_INDEX(col-1)] += qerror*3/16;
                fsd_next_line[FSD_INDEX(col  )] += qerror*5/16;
                fsd_next_line[FSD_INDEX(col+1)] += qerror  /16;
              }
              break;
          }
        } // end pixel
      } // end line
     if(depth == 24) free(fsd_error_buffer);
     gfx_flushBuffer(); // Show results :)
    }
  }
  file.close();
  if (! valid)
  {
    Serial.println(F("Err: BMP"));
  }
}

void drawAnyImageType(const char *filename)
{
    char *ext = strrchr(filename, '.');
    if(!ext)    return; // no extension => we're unable to detect the filetype => we can't call the *corresponding* display method
    ++ext;  // skip '.' itself

    if(strcasecmp(ext, "bmp") == 0)
        drawBitmap_SPIFFS(filename);
    else if(strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0)
        drawJpeg_SPIFFS(filename);
}

void loop(void)
{
    if (SoftAccOK)  dnsServer.processNextRequest(); // DNS server
    server.handleClient();                          // HTTP server

    if(slideshow_is_running &&
       (slideshow_last_switch + SLIDESHOW_PERIOD < millis()))
    {
        if(slideshow_current_index >= slideshow_num_images) slideshow_current_index = 0;
        drawAnyImageType(slideshow_filenames[slideshow_current_index++]);
        slideshow_last_switch = millis();
    }
    else delay(1);    // some pause to lower pointless CPU load
}
