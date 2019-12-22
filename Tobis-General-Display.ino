/*

Tobis General Display

based on: the sketch on the German website https://www.az-delivery.de/blogs/azdelivery-blog-fur-arduino-und-raspberry-pi/captive-portal-blog-teil-4-bmp-dateienanzeige-auf-8x8-matrix-display
by Tobias Kuch

changes/extensions:

- display: black&white only SSD1306 OLED (or else, using u8g2lib) with higher resolution (128x64 e.g.) instead of 8x8 LED matrix wiht colour capability
- smaller images are displayed centered, not in the top left corner
- slideshow function: the MCU can display all suitable images cyclically
- Floyd-Steinberg-Dithering for multicolor images (makes sense on b&w or grayscale display)
- check, which of the colors in a 1-bit image file is lighter
- option (menu item) to display the IP address - instead of an image
- (limited) JPEG capability using the JPEGDecoder lib from Bodmer
- taking some of the configuration out of this main file to the newly created config.h

*/

#include "config.h"
#include <string.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#define FS_NO_GLOBALS       // required ba JPEGDecoder
#include <SPIFFS.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <U8g2lib.h>        // https://github.com/olikraus/u8g2
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
#include <JPEGDecoder.h>    // https://github.com/Bodmer/JPEGDecoder

#define GPIO_OUT_W1TS_REG (DR_REG_GPIO_BASE + 0x0008)
#define GPIO_OUT_W1TC_REG (DR_REG_GPIO_BASE + 0x000c)

// u8g2 object:
U5G2_CONSTRUCTION;

static const byte WiFiPwdLen = WIFIPWDLEN;
static const byte APSTANameLen = APSTANAMELEN;

bool slideshow_is_running = false;
unsigned long slideshow_last_switch = 0;
// unsigned long slideshow_period = SLIDESHOW_PERIOD;  // how long to displayeach image?
int slideshow_current_index = 0;        // next image to display
int slideshow_num_images = 0;
char slideshow_filenames[SLIDESHOW_MAX_IMAGES][MAX_FILENAME_LEN+1];

// these two HTML lines occur several times:
static const char *html_footer = 
       "<footer><p>Programmed and designed by: Tobias Kuch, (u8g2 output:) Arnold Schommer</p>"
       "<p>source hosted at <a href='https://github.com/a-schommer/Tobis-General-Display'>GitHub</a>.</p></footer>";
//       "<footer><p>Programmed and designed by: Tobias Kuch, (SSD1306:) Arnold Schommer</p>"
//       "<p>Contact information: <a href='mailto:tobias.kuch@googlemail.com'>tobias.kuch@googlemail.com</a>.</p></footer>";

struct WiFiEEPromData
  {
    bool APSTA = true; // Access Point or Sation Mode - true AP Mode
    bool PwDReq = false; // PasswordRequired
    bool CapPortal = true ; //CaptivePortal on in AP Mode
    char APSTAName[APSTANameLen]; // STATION /AP Point Name TO cONNECT, if definded
    char WiFiPwd[WiFiPwdLen]; // WiFiPAssword, if definded
    char ConfigValid[3]; //If Config is Vaild, Tag "TK" is required"
  };
#define MAGIC_VALUE_CONFIGVALID "TK"    // must be less than three bytes!!!

struct BMPHeader // BitMapStucture
  {
    uint32_t fileSize;  //
    uint32_t creatorBytes; //
    uint32_t imageOffset; // Start of image data  "Image Offset:
    uint32_t headerSize;     //
    uint32_t width;
    uint32_t height;
    uint16_t planes;
    uint16_t depth; // bits per pixel
    uint32_t format;
  };

/* hostname for mDNS. Should work at least on windows. Try http://<hostname>.local */
const char *ESPHostname = "ESP32";

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;

//Conmmon Paramenters
bool SoftAccOK  = false;

// Web server
WebServer server(80);

/* Soft AP network parameters */
IPAddress apIP(172, 20, 0, 1);
IPAddress netMsk(255, 255, 255, 0);

unsigned long currentMillis = 0;
unsigned long startMillis;

/** Current WLAN status */
short status = WL_IDLE_STATUS;

File fsUploadFile;              // a File object to temporarily store the received file
WiFiEEPromData MyWiFiConfig;
String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)
void handleFileUpload();                // upload a new file to the SPIFFS
String temp ="";

static const byte BRIGHTNESS = 100;     // PresetBrightness
static uint8_t kMatrixWidth, kMatrixHeight;

void setup()
{
  REG_WRITE(GPIO_OUT_W1TS_REG, BIT(GPIO_NUM_16));     // Guru Meditation Error Remediation set
  delay(1);
  REG_WRITE(GPIO_OUT_W1TC_REG, BIT(GPIO_NUM_16));     // Guru Meditation Error Remediation clear
  bool ConnectSuccess = false;
  bool CreateSoftAPSucc  = false;
  bool CInitFSSystem  = false;
  bool CInitHTTPServer  = false;
  byte len;
  Serial.begin(BAUDS);
   while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB
  }
  Serial.println("Serial Interface initalized at "+String(BAUDS)+" Baud.");
  u8g2.begin();
  kMatrixWidth  = u8g2.getDisplayWidth();
  kMatrixHeight = u8g2.getDisplayHeight();
  u8g2.setContrast(BRIGHTNESS);
  u8g2.sendBuffer();
  u8g2.setDrawColor(1);
  WiFi.setAutoReconnect (false);
  WiFi.persistent(false);
  WiFi.disconnect();
  WiFi.setHostname(ESPHostname); // Set the DHCP hostname assigned to ESP station.
  if (loadCredentials()) // Load WLAN credentials for WiFi Settings
  {
     Serial.println(F("Valid Credentials found."));
     MyWiFiConfig.APSTAName[strlen(MyWiFiConfig.APSTAName)+1] = '\0';
     MyWiFiConfig.WiFiPwd[strlen(MyWiFiConfig.WiFiPwd)+1] = '\0';
     if (MyWiFiConfig.APSTA == true)  // AP Mode
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
  { //Set default Config - Create AP
     Serial.println(F("NO Valid Credentials found."));
     SetDefaultWiFiConfig ();
     CreateSoftAPSucc = CreateWifiSoftAP();
     saveCredentials();
     // Blink
     delay(500);
  }
  // Initalize Filesystem
  CInitFSSystem = InitalizeFileSystem();
  if (!(CInitFSSystem)) {Serial.println(F("File System not initalized ! ")); }
  if (ConnectSuccess || CreateSoftAPSucc)
    {
      //Serial.print (F("IP Address: "));
      //if (CreateSoftAPSucc) { Serial.println(WiFi.softAPIP());}
      //if (ConnectSuccess) { Serial.println(WiFi.localIP());}
      InitalizeHTTPServer();
      doShowIP();
    }
    else
    {
      Serial.setDebugOutput(true); //Debug Output for WLAN on Serial Interface.
      Serial.println(F("Error: Cannot connect to WLAN. Set DEFAULT Configuration."));
      SetDefaultWiFiConfig();
      CreateSoftAPSucc = CreateWifiSoftAP();
      InitalizeHTTPServer();
      SetDefaultWiFiConfig();
      saveCredentials();
      u8g2.clear();
      u8g2.sendBuffer();
    }
}

void InitalizeHTTPServer()
 {
  bool initok = false;
  /* Setup web pages: root, wifi config pages, SO captive portal detectors and not found. */
  server.on("/", handleRoot);
  server.on("/wifi", handleWifi);
  server.on("/filesystem", HTTP_GET, handleDisplayFS);
  server.on("/slideshow", HTTP_GET, handleSlideshow);
  server.on("/showip", HTTP_GET, handleShowIP);
  server.on("/upload", HTTP_POST, []() {
  server.send(200, "text/plain", "");
  }, handleFileUpload);
  // if (MyWiFiConfig.CapPortal) { server.on("/generate_204", handleRoot); } //Android captive portal. Maybe not needed. Might be handled by notFound handler.
  // if (MyWiFiConfig.CapPortal) { server.on("/favicon.ico", handleRoot); }   //Another Android captive portal. Maybe not needed. Might be handled by notFound handler. Checked on Sony Handy
  // if (MyWiFiConfig.CapPortal) { server.on("/fwlink", handleRoot); }  //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
  server.on("/generate_204", handleRoot);  //Android captive portal. Maybe not needed. Might be handled by notFound handler.
  server.on("/favicon.ico", handleRoot);    //Another Android captive portal. Maybe not needed. Might be handled by notFound handler. Checked on Sony Handy
  server.on("/fwlink", handleRoot);   //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
  server.onNotFound ( handleNotFound );
  server.begin(); // Web server start
 }

boolean InitalizeFileSystem() {
  bool initok = false;
  initok = SPIFFS.begin();
  delay(200);
  if (!(initok))
  {
    Serial.println(F("Format SPIFFS"));
    SPIFFS.format();
    initok = SPIFFS.begin();
  }
  return initok;
}

boolean CreateWifiSoftAP()
{
  WiFi.disconnect();
  Serial.print(F("Initalize SoftAP "));
  if (MyWiFiConfig.PwDReq)
    {
      SoftAccOK  =  WiFi.softAP(MyWiFiConfig.APSTAName, MyWiFiConfig.WiFiPwd); // Passwortlänge mindestens 8 Zeichen !
    } else
    {
      SoftAccOK  =  WiFi.softAP(MyWiFiConfig.APSTAName); // Access Point WITHOUT Password
      // Overload Function:; WiFi.softAP(ssid, password, channel, hidden)
    }
  delay(2000); // Without delay I've seen the IP address blank
  WiFi.softAPConfig(apIP, apIP, netMsk);
  if (SoftAccOK)
  {
  /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);
  Serial.println(F("successful."));
  } else
  {
  Serial.println(F("Soft AP Error."));
  Serial.println(MyWiFiConfig.APSTAName);
  Serial.println(MyWiFiConfig.WiFiPwd);
  }
  return SoftAccOK;
}

byte ConnectWifiAP()
{
  Serial.println(F("Initalizing Wifi Client."));
  byte connRes = 0;
  byte i = 0;
  WiFi.disconnect();
  WiFi.softAPdisconnect(true); // Function will set currently configured SSID and password of the soft-AP to null values. The parameter  is optional. If set to true it will switch the soft-AP mode off.
  delay(500);
  WiFi.begin(MyWiFiConfig.APSTAName, MyWiFiConfig.WiFiPwd);
  connRes  = WiFi.waitForConnectResult();
  while (( connRes == 0 ) && (i != 10))  //if connRes == 0  "IDLE_STATUS - change Statius"
    {
      connRes  = WiFi.waitForConnectResult();
      delay(2000);
      i++;
      Serial.print(F("."));
      // statement(s)
    }
  while (( connRes == 1 ) && (i != 10))  //if connRes == 1  NO_SSID_AVAILin - SSID cannot be reached
    {
      connRes  = WiFi.waitForConnectResult();
      delay(2000);
      i++;
      Serial.print(F("."));
      // statement(s)
    }
  if (connRes == 3 ) 
  {
     WiFi.setAutoReconnect(true); // Set whether module will attempt to reconnect to an access point in case it is disconnected.
     // Setup MDNS responder
     if (!MDNS.begin(ESPHostname)) {
         Serial.println(F("Error: MDNS"));
     } else { MDNS.addService("http", "tcp", 80); }
  }
  while (( connRes == 4 ) && (i != 10))  //if connRes == 4  Bad Password. Sometimes happens this with corrct PWD
    {
      WiFi.begin(MyWiFiConfig.APSTAName, MyWiFiConfig.WiFiPwd);
      connRes = WiFi.waitForConnectResult();
      delay(2000);
      i++;
      Serial.print(F("."));
    }
  if (connRes == 4 ) 
  {
    Serial.println(F("STA Pwd Err"));
    Serial.println(MyWiFiConfig.APSTAName);
    Serial.println(MyWiFiConfig.WiFiPwd);
    WiFi.disconnect();
  }
Serial.println(F(""));
return connRes;
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
  file =SPIFFS.open(filename, "r");
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
// used like fb[kMatrixHeight][kMatrixWidth]
// reason for this strange (logic) organization:
// i want to keep adjacent x-values adjacent, not adjacent y-values as 
// the dithering walks throuh it line by line, columns iterated in "the inner" loop

bool prepare_framebuffer(const uint16_t width, const uint16_t height)
// width & height: concerning the image, not the framebuffer
{
    if(!fb)
    {   // allocate the framebuffer for full display size
        fb = (int8_t *)malloc(kMatrixWidth*kMatrixHeight*sizeof(*fb));
        if(!fb)
        {
            Serial.println("can't alloc framebuffer, "+String(kMatrixWidth*kMatrixHeight*sizeof(*fb))+" bytes unavailable");
            return false;
        }
    }
    // clear the relevant number of rows, all columns (reducing the columns might save some writes but require a loop...)
    memset((void *)fb, 0, height * kMatrixWidth * sizeof(*fb));
    return true;
}

// draw a tile already loaded to a small memory buffer - called/required by jpegRender()
// converting from RGB565 to grayscale, 0..FS_SCALE_MAX (int8_t)
// CAUTION: will crash, if prepare_framebuffer() is not yet called (successfully) !
void drawRGBTile(uint16_t x, uint16_t y, uint16_t *pImg, uint16_t width, uint16_t height)
{
    // Serial.println("drawRGBTile("+String(x)+", "+String(y)+", *pImg, "+String(width)+", "+String(height)+")");

    while(height >= 0)
    {
        uint16_t cx;
        
        if(y >= kMatrixHeight)  return; // abort if screen is left
 
        for(cx=0; cx<width; ++cx)
        {
            if(x+cx < kMatrixWidth) 
            {   // sum up the rgb parts of the RGB565-value at *pImg to one value:
                int16_t brightness = ((*pImg >> 8) & 0xf8) +  // r
                                     ((*pImg >> 3) & 0xfc) +  // g
                                     ((*pImg << 3) & 0xf8);   // b
                // save the "result", mapped to 0..FS_SCALE_MAX
                fb[x+cx+y*kMatrixWidth] = map(brightness, 0,0xf8+0xfc+0xf8, 0,FS_SCALE_MAX);
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
    uint16_t offset_x = (kMatrixWidth -width )/2,
             offset_y = (kMatrixHeight-height)/2;

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

    u8g2.clear();
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
            int8_t setpoint = fb[col+row*kMatrixWidth] + fsd_this_line[FSD_INDEX(col)],
                   real, qerror;
            if(setpoint > FS_SCALE_MAX/2)
            {
                u8g2.drawPixel(col+offset_x, row+offset_y);
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
    u8g2.sendBuffer(); // Show results :)
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
      uint32_t rowSize = (width * depth / 8 + 3) & ~3;
      if (height < 0)
      {
        height = -height;
        flip = false;
      }
      u8g2.clear();
      uint16_t w = width,  offset_x = (kMatrixWidth-w)/2;
      uint16_t h = height, offset_y = (kMatrixHeight-h)/2;
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
                    u8g2.drawPixel(col+offset_x, row+offset_y);
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
                    u8g2.drawPixel(col+offset_x, row+offset_y);
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
     u8g2.sendBuffer(); // Show results :)
    }
  }
  file.close();
  if (! valid)
  {
    Serial.println(F("Err: BMP"));
  }
}

void handleFileUpload() {
   if (server.uri() != "/upload") return;
   HTTPUpload& upload = server.upload();
   if (upload.status == UPLOAD_FILE_START) {
     String filename = upload.filename;
     if (upload.filename.length() > 30) {
      upload.filename = upload.filename.substring(upload.filename.length() - 30, upload.filename.length());  // Dateinamen auf 30 Zeichen kürzen
    }
     Serial.println("FileUpload Name: " + upload.filename);
     if (!filename.startsWith("/")) filename = "/" + filename;
      fsUploadFile = SPIFFS.open("/" + server.urlDecode(upload.filename), "w");
     filename = String();
   } else if (upload.status == UPLOAD_FILE_WRITE) {
     if (fsUploadFile)
       fsUploadFile.write(upload.buf, upload.currentSize);
   } else if (upload.status == UPLOAD_FILE_END) {
     if (fsUploadFile)
       fsUploadFile.close();
     handleDisplayFS();
   }
 }

void handleDisplayFS() {                     // HTML Filesystem
  //  Page: /filesystem
  temp ="";
  // HTML Header
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  // HTML Content
  server.send ( 200, "text/html", temp );
  temp += "<!DOCTYPE HTML><html lang='de'><head><meta charset='UTF-8'><meta name= viewport content='width=device-width, initial-scale=1.0,'>";
  server.sendContent(temp);
  temp = "";
  temp += "<style type='text/css'><!-- DIV.container { min-height: 10em; display: table-cell; vertical-align: middle }.button {height:35px; width:90px; font-size:16px}";
  server.sendContent(temp);
  temp = "";
  temp += "body {background-color: powderblue;}</style><head><title>" PROJECT_TITLE " - File System Manager</title></head>";
  temp += "<h2>Serial Peripheral Interface Flash Filesystem</h2><body><left>";
  server.sendContent(temp);
  temp = "";
  if (server.args() > 0) // Parameter wurden ubergeben
    {
      if (server.hasArg("delete"))
        {
          String FToDel = server.arg("delete");
          if (SPIFFS.exists(FToDel))
            {
              SPIFFS.remove(FToDel);
              temp += "File " + FToDel + " successfully deleted.";
            } else
            {
              temp += "File " + FToDel + " cannot be deleted.";
            }
          server.sendContent(temp);
          temp = "";
        }
      if (server.hasArg("format") && server.arg("on"))
        {
           SPIFFS.format();
           temp += "SPI File System successfully formatted.";
           server.sendContent(temp);
           temp = "";
        } //   server.client().stop(); // Stop is needed because we sent no content length
    }

  temp += "<table border=2 bgcolor = white width = 400 ><td><h4>Current SPIFFS Status: </h4>";
  temp += formatBytes(SPIFFS.usedBytes() * 1.05) + " of " + formatBytes(SPIFFS.totalBytes()) + " used. <br>";
  temp += formatBytes((SPIFFS.totalBytes() - (SPIFFS.usedBytes() * 1.05)))+ " free. <br>";
  temp += "</td></table><br>";
  server.sendContent(temp);
  temp = "";
  // Check for Site Parameters
  temp += "<table border=2 bgcolor = white width = 400><tr><th><br>";
  temp += "<h4>Available Files on SPIFFS:</h4><table border=2 bgcolor = white ></tr></th><td>Filename</td><td>Size</td><td>Action </td></tr></th>";
  server.sendContent(temp);
  temp = "";
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file)
  {
     temp += "<td> <a title=\"Download\" href =\"" + String(file.name()) + "\" download=\"" + String(file.name()) + "\">" + String(file.name()) + "</a> <br></th>";
     temp += "<td>"+ formatBytes(file.size())+ "</td>";
     temp += "<td><a href =filesystem?delete=" + String(file.name()) + "> Delete </a></td>";
     temp += "</tr></th>";
     file = root.openNextFile();
  }
  temp += "</tr></th>";
  temp += "</td></tr></th><br></th></tr></table></table><br>";
  temp += "<table border=2 bgcolor = white width = 400 ><td><h4>Upload</h4>";
  temp += "<label> Choose File: </label>";
  temp += "<form method='POST' action='/upload' enctype='multipart/form-data' style='height:35px;'><input type='file' name='upload' style='height:35px; font-size:13px;' required>\r\n<input type='submit' value='Upload' class='button'></form>";
  temp += " </table><br>";
  server.sendContent(temp);
  temp = "";
  temp += "<td><a href =filesystem?format=on> Format SPIFFS Filesystem. (Takes up to 30 Seconds) </a></td>";
  temp += "<table border=2 bgcolor = white width = 500 cellpadding =5 ><caption><p><h3>Systemlinks:</h2></p></caption><tr><th><br>";
  temp += " <a href='/'>Main Page</a><br><br></th></tr></table><br><br>";
  server.sendContent(temp);
  temp = "";
  temp += html_footer;
  temp += "</body></html>";
  //server.send ( 200, "", temp );
  server.sendContent(temp);
  server.client().stop(); // Stop is needed because we sent no content length
  temp = "";
 }

/** Load WLAN credentials from EEPROM */

bool loadCredentials()
{
 bool RetValue;
 EEPROM.begin(512);
 EEPROM.get(0, MyWiFiConfig);
 EEPROM.end();
 if (String(MyWiFiConfig.ConfigValid) == String(MAGIC_VALUE_CONFIGVALID))
  {
    RetValue = true;
  } else
  {
    RetValue = false; // WLAN Settings not found.
  }
  return RetValue;
}

/** Store WLAN credentials to EEPROM */
bool saveCredentials()
{
bool RetValue;
// Check logical Errors
RetValue = true;
if  (MyWiFiConfig.APSTA == true ) //AP Mode
  {
   if (MyWiFiConfig.PwDReq && (sizeof(String(MyWiFiConfig.WiFiPwd)) < 8))
    {
      RetValue = false;  // Invalid Config
    }
   if (sizeof(String(MyWiFiConfig.APSTAName)) < 1)
    {
      RetValue = false;  // Invalid Config
    }
  }
if (RetValue)
  {
  EEPROM.begin(512);
  for (int i = 0 ; i < sizeof(MyWiFiConfig) ; i++)
     {
      EEPROM.write(i, 0);
     }
  strncpy( MyWiFiConfig.ConfigValid , MAGIC_VALUE_CONFIGVALID, sizeof(MyWiFiConfig.ConfigValid) );
  EEPROM.put(0, MyWiFiConfig);
  EEPROM.commit();
  EEPROM.end();
  }
  return RetValue;
}


void SetDefaultWiFiConfig()
{
   byte len;
   MyWiFiConfig.APSTA = true;
   MyWiFiConfig.PwDReq = true;  // default PW required
   MyWiFiConfig.CapPortal = true;
   strncpy( MyWiFiConfig.APSTAName, FALLBACK_APSTANAME, sizeof(MyWiFiConfig.APSTAName) );
   len = strlen(MyWiFiConfig.APSTAName);
   MyWiFiConfig.APSTAName[len+1] = '\0';
   strncpy( MyWiFiConfig.WiFiPwd, FALLBACK_WIFIPWD, sizeof(MyWiFiConfig.WiFiPwd) );
   len = strlen(MyWiFiConfig.WiFiPwd);
   MyWiFiConfig.WiFiPwd[len+1] = '\0';
   strncpy( MyWiFiConfig.ConfigValid, MAGIC_VALUE_CONFIGVALID, sizeof(MyWiFiConfig.ConfigValid) );
   len = strlen(MyWiFiConfig.ConfigValid);
   MyWiFiConfig.ConfigValid[len+1] = '\0';
   Serial.println(F("Reset WiFi Credentials."));
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

void handleRoot() {
//  Main Page:
 temp = "";
 short PicCount = 0;
 byte ServArgs = 0;

//Building Page
  // HTML Header
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
// HTML Content
  server.send ( 200, "text/html", temp );   // Speichersparen - Schon mal dem Client senden
  temp = "";
  temp += "<!DOCTYPE HTML><html lang='de'><head><meta charset='UTF-8'><meta name= viewport content='width=device-width, initial-scale=1.0,'>";
  temp += "<style type='text/css'><!-- * { font-family:syns-serif; } DIV.container { min-height: 10em; display: table-cell; vertical-align: middle }.button {height:35px; width:90px; font-size:16px}";
  server.sendContent(temp);
  temp = "";
  temp += "body {background-color: powderblue;}</style>";
  temp += "<head><title>" PROJECT_TITLE "</title></head>";
  temp += "<h2>LED Display</h2>";
  temp += "<body>";
  server.sendContent(temp);
  temp = "";
// Processing User Request
if (server.args() > 0) // Parameter wurden ubergeben
{
  temp += "<br>Eingaben werden verarbeitet. Bitte warten..<br><br>";;
  server.sendContent(temp);
  temp = "";
  if(server.hasArg("PicSelect"))
  {
    if (server.arg("PicSelect") == "off")  // Clear Display
      {
        u8g2.clear();
        u8g2.sendBuffer();
      } 
    else
      {
        drawAnyImageType(server.arg("PicSelect").c_str()); // Bild gewählt. Display inhalt per Picselect hergstellt
      }
  }
}
  temp += "<table border=2 bgcolor = white ><caption><p><h3>Available Pictures in SPIIFS for "+String(kMatrixWidth)+"*"+String(kMatrixHeight)+" Display</h2></p></caption>";
  temp += "<form>";
  temp += "<tr><th><input type='radio' name='PicSelect' value = 'off' checked> Clear Display<br></th></tr>";
  temp += "<tr><th>";
  //List available BMP Files in SPIFFS
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  PicCount = 1;
  while (file)
   {
    char *ext = strrchr(file.name(), '.');
    if(ext) ++ext;  // skip '.' itself, but not if not found!
    if(strcasecmp(ext, "bmp") == 0)
    {
      BMPHeader PicData = ReadBitmapSpecs(file.name());
      if (((PicData.width <= kMatrixWidth) && (PicData.height <= kMatrixHeight)) && // Display only in list, when Bitmap not exceeding Display Resolution. Bigger Images are not listed. ...
          ((PicData.depth == 1) || (PicData.depth == 24)))                          // ... and when the bitmap has a known/understood bitdepth.
        {
          temp += "<label for='radio1'><img src='"+ String(file.name())+"' alt='"+ String(file.name())+"' border='3' bordercolor=green> Image "+ PicCount+"</label><input type='radio' value='"+ String(file.name())+"' name='PicSelect'/> <br>";
          temp += String(file.name())+ " "+ String(PicData.width) + "*" + String(PicData.height) + "px*" + String(PicData.depth) + "bit; filesize: "+ formatBytes(file.size()) + "</th></tr><tr><th>";
          strncpy(slideshow_filenames[PicCount-1], file.name(), MAX_FILENAME_LEN+1);
          PicCount ++;
        }
    }
    else if ((strcasecmp(ext, "jpg") == 0) || (strcasecmp(ext, "jpeg") == 0))
        if(JpegDec.decodeFsFile(file.name()))                                       // Display only in list, when file could be decoded ...
            if(JpegDec.width <= kMatrixWidth && JpegDec.height <= kMatrixHeight)    // ... and does not exceed the display size
            {
                temp += "<label for='radio1'><img src='"+ String(file.name())+"' alt='"+ String(file.name())+"' border='3' bordercolor=green> Image "+ PicCount+"</label><input type='radio' value='"+ String(file.name())+"' name='PicSelect'/> <br>";
                temp += String(file.name())+ " "+ String(JpegDec.width) + "*" + String(JpegDec.height) + "px; filesize: "+ formatBytes(file.size()) + "</th></tr><tr><th>";
                strncpy(slideshow_filenames[PicCount-1], file.name(), MAX_FILENAME_LEN+1);
                PicCount ++;
            }
   file = root.openNextFile();
   }
  server.sendContent(temp);
  slideshow_num_images = PicCount - 1;
  temp = "";
  temp = "<button type='submit' name='action' value='0' style='height: 50px; width: 280px'>Show Image on Display</button>";
  temp += "</form>";
  temp += "<br><table border=2 bgcolor = white width = 280 cellpadding =5 ><caption><p><h3>Systemlinks:</h2></p></caption>";
  temp += "<tr><td><br>";
  temp += "<a href='/wifi'>WIFI Settings</a><br><br>";
  temp += "<a href='/filesystem'>Filemanager</a><br><br>";
  if(slideshow_num_images > 1)
  {
      temp += slideshow_is_running ? "<a href='/slideshow?off=1'>stop slideshow</a><br><br>" : "<a href='/slideshow?on=1'>start slideshow</a><br><br>";
  }
  temp += "<a href='/showip'>show ip (on display)</a><br><br>";
  temp += "</td></tr></table><br><br>";
  temp += html_footer;
  temp += "</body></html>";
  server.sendContent(temp);
  temp = "";
  server.client().stop(); // Stop is needed because we sent no content length
}

void handleNotFound() {
    if (captivePortal())
      { // If captive portal redirect instead of displaying the error page.
        return;
      }
  if (!handleFileRead(server.uri()))
    {
    temp = "";
    // HTML Header
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    // HTML Content
    temp += "<!DOCTYPE HTML><html lang='de'><head><meta charset='UTF-8'><meta name= viewport content='width=device-width, initial-scale=1.0,'>";
    temp += "<style type='text/css'><!-- DIV.container { min-height: 10em; display: table-cell; vertical-align: middle }.button {height:35px; width:90px; font-size:16px}";
    temp += "body {background-color: powderblue;}</style>";
    temp += "<head><title>" PROJECT_TITLE " - File not found</title></head>";
    temp += "<h2> 404 File Not Found</h2><br>";
    temp += "<h4>Debug Information:</h4><br>";
    temp += "<body>";
    temp += "URI: ";
    temp += server.uri();
    temp += "\nMethod: ";
    temp+= ( server.method() == HTTP_GET ) ? "GET" : "POST";
    temp += "<br>Arguments: ";
    temp += server.args();
    temp += "\n";
      for ( uint8_t i = 0; i < server.args(); i++ ) {
        temp += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
        }
    temp += "<br>Server Hostheader: "+ server.hostHeader();
    for ( uint8_t i = 0; i < server.headers(); i++ ) {
        temp += " " + server.headerName ( i ) + ": " + server.header ( i ) + "\n<br>";
        }
    temp += "</table></form><br><br><table border=2 bgcolor = white width = 500 cellpadding =5 ><caption><p><h2>You may want to browse to:</h2></p></caption>";
    temp += "<tr><th>";
    temp += "<a href='/'>Main Page</a><br>";
    temp += "<a href='/wifi'>WIFI Settings</a><br>";
    temp += "<a href='/filesystem'>Filemanager</a><br>";
    temp += "</th></tr></table><br><br>";
    temp += html_footer;
    temp += "</body></html>";
    server.send ( 404, "", temp );
    server.client().stop(); // Stop is needed because we sent no content length
    temp = "";
    }
}

/** Redirect to captive portal if we got a request for another domain. Return true in that case so the page handler do not try to handle the request again. */
boolean captivePortal() {
  if (!isIp(server.hostHeader()) && server.hostHeader() != (String(ESPHostname)+".local")) {
    // Serial.println("Request redirected to captive portal");
    server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
    server.send ( 302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

/** Wifi config page handler */
void handleWifi()
 {
  //  Page: /wifi
  byte i;
  byte len ;
  temp = "";
  // Check for Site Parameters
      if (server.hasArg("Reboot") )  // Reboot System
       {
         temp = "Rebooting System in 5 Seconds..";
         server.send ( 200, "text/html", temp );
         delay(5000);
         server.client().stop();
         WiFi.disconnect();
         delay(1000);
       }

      if (server.hasArg("WiFiMode") && (server.arg("WiFiMode") == "1")  )  // STA Station Mode Connect to another WIFI Station
       {
        startMillis = millis(); // Reset Time Up Counter to avoid Idle Mode whiole operating
        // Connect to existing STATION
        if ( sizeof(server.arg("WiFi_Network")) > 0  )
          {
            Serial.println("STA Mode");
            MyWiFiConfig.APSTA = false; // Access Point or Station Mode - false Station Mode
            temp = "";
            for ( i = 0; i < APSTANameLen;i++) { MyWiFiConfig.APSTAName[i] =  0; }
            temp = server.arg("WiFi_Network");
            len =  temp.length();
            for ( i = 0; i < len;i++)
            {
                  MyWiFiConfig.APSTAName[i] =  temp[i];
            }
         //   MyWiFiConfig.APSTAName[len+1] = '\0';
            temp = "";

            for ( i = 0; i < WiFiPwdLen;i++)  { MyWiFiConfig.WiFiPwd[i] =  0; }
            temp = server.arg("STAWLanPW");
            len =  temp.length();
            for ( i = 0; i < len;i++)
              {
                if (temp[i] > 32) //Steuerzeichen raus
                  {
                   MyWiFiConfig.WiFiPwd[i] =  temp[i];
                  }
              }
        //    MyWiFiConfig.WiFiPwd[len+1] = '\0';
            temp = "WiFi Connect to AP: -";
            temp += MyWiFiConfig.APSTAName;
            temp += "-<br>WiFi PW: -";
            temp += MyWiFiConfig.WiFiPwd;
            temp += "-<br>";
            temp += "Connecting to STA Mode in 2 Seconds..<br>";
            server.send ( 200, "text/html", temp );
            server.sendContent(temp);
            delay(2000);
            server.client().stop();
            server.stop();
            temp = "";
            WiFi.disconnect();
            WiFi.softAPdisconnect(true);
            delay(500);
           // ConnectWifiAP
           bool SaveOk = saveCredentials();
            i = ConnectWifiAP();
            delay(700);
            if (i != 3) // 4: WL_CONNECT_FAILED - Password is incorrect 1: WL_NO_SSID_AVAILin - Configured SSID cannot be reached
              {
                 Serial.print(F("Cannot Connect to specified Network. Reason: "));
                 Serial.println(i);
                 server.client().stop();
                 delay(100);
                 WiFi.setAutoReconnect (false);
                 delay(100);
                 WiFi.disconnect();
                 delay(1000);
                 SetDefaultWiFiConfig();
                 CreateWifiSoftAP();
                 return;
              } else
              {
                 // Safe Config
                 bool SaveOk = saveCredentials();
                 InitalizeHTTPServer();
                 return;
              }
          }
       }

      if (server.hasArg("WiFiMode") && (server.arg("WiFiMode") == "2")  )  // Change AP Mode
       {
        startMillis = millis(); // Reset Time Up Counter to avoid Idle Mode whiole operating
        // Configure Access Point
        temp = server.arg("APPointName");
        len =  temp.length();
        temp = server.arg("APPW");
        i = server.hasArg("PasswordReq") ? temp.length() : 8;

        if (  ( len > 1 ) && (server.arg("APPW") == server.arg("APPWRepeat")) && ( i > 7)          )
          {
            temp = "";
            Serial.println(F("APMode"));
            MyWiFiConfig.APSTA = true; // Access Point or Sation Mode - true AP Mode
            MyWiFiConfig.CapPortal = server.hasArg("CaptivePortal");
            MyWiFiConfig.PwDReq = server.hasArg("PasswordReq");

            for ( i = 0; i < APSTANameLen;i++) { MyWiFiConfig.APSTAName[i] =  0; }
            temp = server.arg("APPointName");
            len =  temp.length();
            for ( i = 0; i < len;i++) { MyWiFiConfig.APSTAName[i] =  temp[i]; }
            MyWiFiConfig.APSTAName[len+1] = '\0';
            temp = "";
            for ( i = 0; i < WiFiPwdLen;i++)  {  MyWiFiConfig.WiFiPwd[i] =  0; }
            temp = server.arg("APPW");
            len =  temp.length();
            for ( i = 0; i < len;i++)  { MyWiFiConfig.WiFiPwd[i] =  temp[i];  }
            MyWiFiConfig.WiFiPwd[len+1] = '\0';
            temp = "";
            temp = saveCredentials() ? // Save AP ConfigCongfig
                    "Daten des AP Modes erfolgreich gespeichert. Reboot notwendig." :
                    "Daten des AP Modes fehlerhaft.";
          } else temp = (server.arg("APPW") != server.arg("APPWRepeat")) ? 
                  "WLAN Passwort nicht gleich. Abgebrochen." :
                  "WLAN Passwort oder AP Name zu kurz. Abgebrochen.";
       // End WifiAP
       }
  // HTML Header
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
// HTML Content
  temp += "<!DOCTYPE HTML><html lang='de'><head><meta charset='UTF-8'><meta name= viewport content='width=device-width, initial-scale=1.0,'>";
  server.send ( 200, "text/html", temp );
  temp = "";
  temp += "<style type='text/css'><!-- DIV.container { min-height: 10em; display: table-cell; vertical-align: middle }.button {height:35px; width:90px; font-size:16px}";
  temp += "body {background-color: powderblue;}</style><head><title>" PROJECT_TITLE " - WiFi Settings</title></head>";
  server.sendContent(temp);
  temp = "";
  temp += "<h2>WiFi Settings</h2><body><left>";
  temp += "<table border=2 bgcolor = white width = 500 ><td><h4>Current WiFi Settings: </h4>";
  if (server.client().localIP() == apIP) {
     temp += "Mode : Soft Access Point (AP)<br>";
     temp += "SSID : " + String (MyWiFiConfig.APSTAName) + "<br><br>";
  } else {
     temp += "Mode : Station (STA) <br>";
     temp += "SSID  :  "+ String (MyWiFiConfig.APSTAName) + "<br>";
     temp += "BSSID :  " + WiFi.BSSIDstr()+ "<br><br>";
  }
  temp += "</td></table><br>";
  server.sendContent(temp);
  temp = "";
  temp += "<form action='/wifi' method='post'>";
  temp += "<table border=2 bgcolor = white width = 500><tr><th><br>";
  if (MyWiFiConfig.APSTA == 1)
    {
      temp += "<input type='radio' value='1' name='WiFiMode' > WiFi Station Mode<br>";
    } else
    {
      temp += "<input type='radio' value='1' name='WiFiMode' checked > WiFi Station Mode<br>";
    }
  temp += "Available WiFi Networks:<table border=2 bgcolor = white ></tr></th><td>Number </td><td>SSID  </td><td>Encryption </td><td>WiFi Strength </td>";
  server.sendContent(temp);
  temp = "";
  WiFi.scanDelete();
  int n = WiFi.scanNetworks(false, false); //WiFi.scanNetworks(async, show_hidden)
  if (n > 0) {
    for (int i = 0; i < n; i++) {
    temp += "</tr></th>";
    String Nrb = String(i);
    temp += "<td>" + Nrb + "</td>";
    temp += "<td>" + WiFi.SSID(i) +"</td>";

    Nrb = GetEncryptionType(WiFi.encryptionType(i));
    temp += "<td>"+ Nrb + "</td>";
    temp += "<td>" + String(WiFi.RSSI(i)) + "</td>";
    }
  } else {
    temp += "</tr></th>";
    temp += "<td>1 </td>";
    temp += "<td>No WLAN found</td>";
    temp += "<td> --- </td>";
    temp += "<td> --- </td>";
  }
  temp += "</table><table border=2 bgcolor = white ></tr></th><td>Connect to WiFi SSID: </td><td><select name='WiFi_Network' >";
if (n > 0) {
    for (int i = 0; i < n; i++) {
    temp += "<option value='" + WiFi.SSID(i) +"'>" + WiFi.SSID(i) +"</option>";
    }
  } else {
    temp += "<option value='No_WiFi_Network'>No WiFiNetwork found !/option>";
  }
  server.sendContent(temp);
  temp = "";
  temp += "</select></td></tr></th></tr></th><td>WiFi Password: </td><td>";
  temp += "<input type='text' name='STAWLanPW' maxlength='40' size='40'>";
  temp += "</td></tr></th><br></th></tr></table></table><table border=2 bgcolor = white width = 500 ><tr><th><br>";
  server.sendContent(temp);
  temp = "";
  if (MyWiFiConfig.APSTA == true)
    {
      temp += "<input type='radio' name='WiFiMode' value='2' checked> WiFi Access Point Mode <br>";
    } else
    {
      temp += "<input type='radio' name='WiFiMode' value='2' > WiFi Access Point Mode <br>";
    }
  temp += "<table border=2 bgcolor = white ></tr></th> <td>WiFi Access Point Name: </td><td>";
  server.sendContent(temp);
  temp = "";
  if (MyWiFiConfig.APSTA == true)
    {
      temp += "<input type='text' name='APPointName' maxlength='"+String(APSTANameLen-1)+"' size='30' value='" + String(MyWiFiConfig.APSTAName) + "'></td>";
    } else
    {
      temp += "<input type='text' name='APPointName' maxlength='"+String(APSTANameLen-1)+"' size='30' ></td>";
    }
  server.sendContent(temp);
  temp = "";
  if (MyWiFiConfig.APSTA == true)
    {
      temp += "</tr></th><td>WiFi Password: </td><td>";
      temp += "<input type='password' name='APPW' maxlength='"+String(WiFiPwdLen-1)+"' size='30' value='" + String(MyWiFiConfig.WiFiPwd) + "'> </td>";
      temp += "</tr></th><td>Repeat WiFi Password: </td>";
      temp += "<td><input type='password' name='APPWRepeat' maxlength='"+String(WiFiPwdLen-1)+"' size='30' value='" + String(MyWiFiConfig.WiFiPwd) + "'> </td>";
    } else
    {
      temp += "</tr></th><td>WiFi Password: </td><td>";
      temp += "<input type='password' name='APPW' maxlength='"+String(WiFiPwdLen-1)+"' size='30'> </td>";
      temp += "</tr></th><td>Repeat WiFi Password: </td>";
      temp += "<td><input type='password' name='APPWRepeat' maxlength='"+String(WiFiPwdLen-1)+"' size='30'> </td>";
    }
      temp += "</table>";
  server.sendContent(temp);
  temp = "";
  if (MyWiFiConfig.PwDReq)
    {
      temp += "<input type='checkbox' name='PasswordReq' checked> Password for Login required. ";
    } else
    {
      temp += "<input type='checkbox' name='PasswordReq' > Password for Login required. ";
    }
  server.sendContent(temp);
  temp = "";
  if (MyWiFiConfig.CapPortal)
    {
      temp += "<input type='checkbox' name='CaptivePortal' checked> Activate Captive Portal";
    } else
    {
      temp += "<input type='checkbox' name='CaptivePortal' > Activate Captive Portal";
    }
  server.sendContent(temp);
  temp = "";
  temp += "<br></tr></th></table><br> <button type='submit' name='Settings' value='1' style='height: 50px; width: 140px' autofocus>Set WiFi Settings</button>";
  temp += "<button type='submit' name='Reboot' value='1' style='height: 50px; width: 200px' >Reboot System</button>";
  server.sendContent(temp);
  temp = "";
  temp += "<button type='reset' name='action' value='1' style='height: 50px; width: 100px' >Reset</button></form>";
  temp += "<table border=2 bgcolor = white width = 500 cellpadding =5 ><caption><p><h3>Systemlinks:</h2></p></caption><tr><th><br>";
  server.sendContent(temp);
  temp = "";
  temp += "<a href='/'>Main Page</a><br><br></th></tr></table><br><br>";
  temp += html_footer;
  temp += "</body></html>";
  server.sendContent(temp);
  server.client().stop(); // Stop is needed because we sent no content length
  temp = "";
}

void handleUploadSave()
{
  String FileData ;
  temp = "";
  for (byte i = 0; i < server.args(); i++)
  {
    temp += "Arg " + (String)i + " –> ";   //Include the current iteration value
    temp += server.argName(i) + ": ";     //Get the name of the parameter
    temp += server.arg(i) + "\n";              //Get the value of the parameter
  }
  // server.send(200, "text/plain", temp);       //Response to the HTTP request
  FileData = server.arg("datei");
  server.sendHeader("Location", "filesystem", true);
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send ( 302, "text/plain", "");  // Empty content inhibits Content-length header so we have to close the socket ourselves.
  server.client().stop(); // Stop is needed because we sent no content length
}

void handleSlideshow()
{
    if(server.hasArg("on"))
    {
        Serial.println("Slideshow on");
        slideshow_is_running = true;
        slideshow_last_switch = slideshow_current_index = 0;
    } else if(server.hasArg("off"))
    {
        Serial.println("Slideshow off");
        slideshow_is_running = false;
    }
    else Serial.println("Slideshow ???");

    server.sendHeader("Location", "/", true);
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send ( 302, "text/plain", "");  // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop(); // Stop is needed because we sent no content length
}

void doShowIP(void)
{   IPAddress myIP = WiFi.softAPIP();
    
    if((uint32_t)myIP == 0)
    {
        myIP = WiFi.localIP();
        Serial.print("WiFi.localIP() = ");
    }
    else
        Serial.print("WiFi.softAPIP() = ");
    Serial.println(myIP);
        
    u8g2.clear();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(0,32, myIP.toString().c_str());
    u8g2.sendBuffer();
}

void handleShowIP()
{
    doShowIP();
    // "suspend" the slideshow for one cycle (no need to check if it is running)
    slideshow_last_switch = millis();
    
    server.sendHeader("Location", "/", true);
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send ( 302, "text/plain", "");  // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop(); // Stop is needed because we sent no content length
}

/** Is this an IP? */
boolean isIp(String str) {
  for (int i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

String GetEncryptionType(byte thisType) {
  String Output = "";
   // read the encryption type and print out the name:
   switch (thisType) {
     case 5:
       Output = "WEP";
       return Output;
       break;
     case 2:
       Output = "WPA";
       return Output;
       break;
     case 4:
       Output = "WPA2";
       return Output;
       break;
     case 7:
       Output = "None";
       return Output;
       break;
     case 8:
       Output = "Auto";
       return Output;
      break;
   }
}

/** IP to String? */
String toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

String formatBytes(size_t bytes) {            // lesbare Anzeige der Speichergrößen
   if (bytes < 1024) {
     return String(bytes) + " Bytes";
   } else if (bytes < (1024 * 1024)) {
     return String(bytes / 1024.0) + " KB";
   } else {
     return String(bytes / 1024.0 / 1024.0) + " MB";
   }
 }

String getContentType(String filename) { // convert the file extension to the MIME type
  char *ext = strrchr(filename.c_str(), '.');
  if(ext) ++ext;    // skip '.' itself, but not if not found!

  if      (strcasecmp(ext, "htm")  == 0 ||
           strcasecmp(ext, "html") == 0)    return "text/html";
  else if (strcasecmp(ext, "css") == 0)     return "text/css";
  else if (strcasecmp(ext, "js") == 0)      return "application/javascript";
  else if (strcasecmp(ext, "ico") == 0)     return "image/x-icon";
  else if (strcasecmp(ext, "gz") == 0)      return "application/x-gzip";
  else if (strcasecmp(ext, "bmp") == 0)     return "image/bmp";
  else if (strcasecmp(ext, "tif") == 0)     return "image/tiff";
  else if (strcasecmp(ext, "pbm") == 0)     return "image/x-portable-bitmap";
  else if (strcasecmp(ext, "jpg")  == 0 ||
           strcasecmp(ext, "jpeg") == 0 )   return "image/jpeg";
  else if (strcasecmp(ext, "gif") == 0)     return "image/gif";
  else if (strcasecmp(ext, "png") == 0)     return "image/png";
  else if (strcasecmp(ext, "svg") == 0)     return "image/svg+xml";
  else if (strcasecmp(ext, "wav") == 0)     return "audio/x-wav";
  else if (strcasecmp(ext, "zip") == 0)     return "application/zip";
  else if (strcasecmp(ext, "rgb") == 0)     return "image/x-rg";
 // Complete List on https://wiki.selfhtml.org/wiki/MIME-Type/Übersicht
  return "text/plain";
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                       // Use the compressed version
    File file = SPIFFS.open(path, "r");                    // Open the file
    server.streamFile(file, contentType);                  // Send it to the client
    file.close();                                          // Close the file again
    return true;
  }
  return false;
}

void loop()
 {
  if (SoftAccOK)
  {
    dnsServer.processNextRequest(); //DNS
  }
  //HTTP
  server.handleClient();

  if(slideshow_is_running) {
      if(slideshow_last_switch + SLIDESHOW_PERIOD < millis()) {
          if(slideshow_current_index >= slideshow_num_images) slideshow_current_index = 0;
          drawAnyImageType(slideshow_filenames[slideshow_current_index++]);
          slideshow_last_switch = millis();
      }
  }
  else delay(1);    // some pause to lower useless CPU load
}
