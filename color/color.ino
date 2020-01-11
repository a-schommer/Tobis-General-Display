/*

Tobis General Display
by Arnold Schommer, Tobias Kuch

color.ino - main sketch, ucg variant

based on: the sketch on the German website https://www.az-delivery.de/blogs/azdelivery-blog-fur-arduino-und-raspberry-pi/captive-portal-blog-teil-4-bmp-dateienanzeige-auf-8x8-matrix-display
by Tobias Kuch

changes/extensions:

- smaller images are displayed centered, not in the top left corner
- slideshow function: the MCU can display all suitable images cyclically
- check, which of the colors in a 1-bit image file is lighter
- option (menu item) to display the IP address - instead of an image
- (limited) JPEG capability using the JPEGDecoder lib from Bodmer
- taking some of the configuration out of this main file to the newly created config.h
- display: anything supported by ucglib by olikraus instead of 8x8 LED matrix with colour capability
- display of IP adddress, SSID & WiFi password can be configured in the EEPROM-data

*/

#include "pre-config.h"
#include "config.h"
#include <string.h>
#include "esplayer.h"
#include <DNSServer.h>
#include "settings.h"
#include <Ucglib.h>         // https://github.com/olikraus/ucglib
#include "bitmap.h"
#include <JPEGDecoder.h>    // https://github.com/Bodmer/JPEGDecoder
#include "network.h"

// ucg object:
UCG_CONSTRUCTION;
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

inline void readrgb(File f, uint8_t *r, uint8_t *g, uint8_t *b)
{   // read a triple rgb plus a fourth dummy and just return the sum:
    *b = f.read();
    *g = f.read();
    *r = f.read();
    f.read();   // skip fourth byte to enable reading the next palette entry
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
// draw a tile already loaded to a small memory buffer - called/required by jpegRender()
// colors are "recieved" as RGB565
void drawRGBTile(int16_t x, int16_t y, uint16_t *pImg, int16_t width, int16_t height)
{
    // Serial.println("drawRGBTile("+String(x)+", "+String(y)+", *pImg, "+String(width)+", "+String(height)+")");

    while(height > 0)
    {
        uint16_t cx;

        if(y >= gfx_getScreenHeight())  return; // abort if screen is left

        for(cx=0; cx<width; ++cx)
        {
            if(x+cx < gfx_getScreenWidth())
            {
                uint8_t red   = ((*pImg >> 8) & 0xf8),
                        green = ((*pImg >> 3) & 0xfc),
                        blue  = ((*pImg << 3) & 0xf8);
                gfx_setPixelColor(red, green, blue);
                gfx_setPixel(x+cx, y);
            }
            ++pImg; // advance in the buffer
        }
        // advance to next row
        ++y; --height;
    }
}

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
      uint8_t red0, green0, blue0, red1, green1, blue1;

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

      if(depth == 1)
      {   // read the palette
          file.seek(5*sizeof(uint32_t),SeekCur);    // skip remainder of header, go to start of color table
          readrgb(file, &red0, &green0, &blue0);
          readrgb(file, &red1, &green1, &blue1);
      }
      uint32_t rowSize = (width * depth / 8 + 3) & ~3;
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
                if (0 == col % 8)
                    bits = buffer[buffidx++];
                if(bits & 0x80)
                    gfx_setPixelColor(red1, green1, blue1);
                else
                    gfx_setPixelColor(red0, green0, blue0);
                bits <<= 1;
                break;
            case 24: // standard BMP format
                blue0  = buffer[buffidx++];
                green0 = buffer[buffidx++],
                red0   = buffer[buffidx++];
                gfx_setPixelColor(red0, green0, blue0);
                break;
          }
          gfx_setPixel(col+offset_x, row+offset_y);
        } // end pixel
      } // end line
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
