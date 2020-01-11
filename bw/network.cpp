/*

Tobis General Display
by Arnold Schommer, Tobias Kuch

network.cpp - network (WiFi, HTTP-UI) related stuff, implementation

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
#include "network.h"
extern U8G2_DECLARATION;
#include "gfxlayer.h"
#include "bitmap.h"
#include <JPEGDecoder.h>    // https://github.com/Bodmer/JPEGDecoder

/*********************************************************************/
// "imported" from the main sketch:
extern bool slideshow_is_running;
extern unsigned long slideshow_last_switch;
extern int slideshow_current_index;
void drawAnyImageType(const char *filename);
/*********************************************************************/

// CSS is used several times:
static const char *css_definition = "<style type='text/css'><!-- * {font-family:sans-serif;} "
        "DIV.container {min-height: 10em; display: table-cell; vertical-align: middle} "
        ".button {height:35px; width:90px; font-size:16px} "
        "body {background-color: powderblue;} --></style>";
// these two HTML lines occur several times:
static const char *html_footer =
       "<footer><p>Programmed and designed by: Tobias Kuch,<br>(u8g2/ucglib output:) Arnold Schommer</p>"
       "<p>source hosted at <a href='https://github.com/a-schommer/Tobis-General-Display'>GitHub</a>.</p></footer>";

/* hostname for mDNS. Should work at least on windows. Try http://<hostname>.local */
const char *ESPHostname = FALLBACK_HOSTNAME;

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;

//Conmmon Paramenters
bool SoftAccOK  = false;

// Web server
WEBSERVER_CLASS server(80);

/* Soft AP network parameters */
IPAddress apIP(172, 20, 0, 1);
IPAddress netMsk(255, 255, 255, 0);

// Current WiFi status
short status = WL_IDLE_STATUS;

File fsUploadFile;              // a File object to temporarily store the received file

int slideshow_num_images = 0;
char slideshow_filenames[SLIDESHOW_MAX_IMAGES][MAX_FILENAME_LEN+1];

boolean CreateWifiSoftAP(void)
{
  WiFi.disconnect();
  Serial.print(F("Initialize SoftAP "));
  if (SETTINGS_IS_WIFI_PASSWORD_REQUIRED)
    {
      SoftAccOK = WiFi.softAP(MySettings.WiFiAPSTAName, MySettings.WiFiPwd); // password length at least 8 characters!
    } else
    {
      SoftAccOK = WiFi.softAP(MySettings.WiFiAPSTAName); // access point WITHOUT password
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
    Serial.println(MySettings.WiFiAPSTAName);
    Serial.println(MySettings.WiFiPwd);
  }
  doShowWifi(false);
  return SoftAccOK;
}

byte ConnectWifiAP(void)
{
  Serial.println(F("initializing Wifi client"));
  byte connRes = 0;
  byte i = 0;
  WiFi.disconnect();
  WiFi.softAPdisconnect(true); // Function will set currently configured SSID and password of the soft-AP to null values. The parameter  is optional. If set to true it will switch the soft-AP mode off.
  delay(500);
  WiFi.begin(MySettings.WiFiAPSTAName, MySettings.WiFiPwd);
  connRes  = WiFi.waitForConnectResult();
  while (( connRes == 0 ) && (i < 10))  //if connRes == 0  "IDLE_STATUS - change Statius"
    {
      connRes  = WiFi.waitForConnectResult();
      delay(2000);
      i++;
      Serial.print(F("."));
      // statement(s)
    }
  while (( connRes == 1 ) && (i < 10))  //if connRes == 1  NO_SSID_AVAILin - SSID cannot be reached
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
  while (( connRes == 4 ) && (i < 10))  //if connRes == 4  Bad Password. Sometimes happens this with corrct PWD
    {
      WiFi.begin(MySettings.WiFiAPSTAName, MySettings.WiFiPwd);
      connRes = WiFi.waitForConnectResult();
      delay(2000);
      i++;
      Serial.print(F("."));
    }
  if (connRes == 4 )
  {
    Serial.println(F("STA Pwd Err"));
    Serial.println(MySettings.WiFiAPSTAName);
    Serial.println(MySettings.WiFiPwd);
    WiFi.disconnect();
  }
Serial.println(F(""));
return connRes;
}

// mask special characters, returning a pseudo-copy - in fact, to a static buffer...
char *urlencode(char const *from)
{
    static char buffer[2*MAX_FILENAME_LEN+1];   // not safely enough, but ...
    char *to;

    for(to=buffer; *from; ++from) {
        char org = *from;
        if(((org >= 'a') && (org <='z')) ||
           ((org >= 'A') && (org <='Z')) ||
           ((org >= '0') && (org <='9')) ||
           (org == '.') || (org == '_') || (org == '-') || (org == '~')) *to++ = org;
        else { static char hexdigits[] = "0123456789ABCDEF";
            *to++ = '%';
            *to++ = hexdigits[(org >> 4) & 0x0f];
            *to++ = hexdigits[org & 0x0f];
        }
    }
    *to = 0;
    return buffer;
}

void handleFileUpload(void)
{
   if (server.uri() != "/upload") return;
   HTTPUpload& upload = server.upload();
   if (upload.status == UPLOAD_FILE_START) {
     String filename = upload.filename;
     if (upload.filename.length() > 30) {
      upload.filename = upload.filename.substring(upload.filename.length() - 30, upload.filename.length());  // shorten filename to 30 chars
     }
     Serial.println("FileUpload Name: " + upload.filename);
     if (!filename.startsWith("/")) filename = "/" + filename;
     fsUploadFile = SPIFFS.open("/" + server.urlDecode(upload.filename), "w");
     filename = String();
   } else if (upload.status == UPLOAD_FILE_WRITE) {
     if (fsUploadFile)  fsUploadFile.write(upload.buf, upload.currentSize);
   } else if (upload.status == UPLOAD_FILE_END) {
     if (fsUploadFile)  fsUploadFile.close();
     handleDisplayFS();
   }
 }

void handleDisplayFS(void)                       //  Page: /filesystem
{
  String temp ="";
  // HTML Header
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  // HTML Content
  server.send ( 200, "text/html", temp );
  temp += "<!DOCTYPE HTML><html lang='de'><head><meta charset='UTF-8'><meta name= viewport content='width=device-width, initial-scale=1.0,'>";
  server.sendContent(temp);
  server.sendContent(css_definition);
  temp = "";
  temp += "<title>" PROJECT_TITLE " - File System Manager</title></head>";
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
  { size_t usedBytes  = esp_get_fs_usedBytes() * 1.05,
           totalBytes = esp_get_fs_totalBytes();
  temp += formatBytes(usedBytes) + " of " + formatBytes(totalBytes) + " used. <br>";
  temp += formatBytes(totalBytes - usedBytes)+ " free. <br>";
  }
  temp += "</td></table><br>";
  server.sendContent(temp);
  temp = "";
  // Check for Site Parameters
  temp += "<table border=2 bgcolor = white width = 400><tr><th><br>";
  temp += "<h4>Available Files on SPIFFS:</h4><table border=2 bgcolor = white ></tr></th><td>Filename</td><td>Size</td><td>Action </td></tr></th>";
  server.sendContent(temp);
  temp = "";
  ESP_CLASS_DIR root = esp_openDir("/");
  File file;
  while (file = esp_openNextFile(root))
  {
     temp += "<td> <a title=\"Download\" href =\"" + String(file.name()) + "\" download=\"" + String(file.name()) + "\">" + String(file.name()) + "</a> <br></th>";
     temp += "<td>"+ formatBytes(file.size())+ "</td>";
     temp += "<td><a href=filesystem?delete=" + String(urlencode(file.name())) + "> Delete </a></td>";
     temp += "</tr></th>";
  }
  temp += "</tr></th>";
  temp += "</td></tr></th><br></th></tr></table></table><br>";
  temp += "<table border=2 bgcolor=white width=400><td><h4>Upload</h4>";
  temp += "<label> Choose File: </label>";
  temp += "<form method='POST' action='/upload' enctype='multipart/form-data' style='height:35px;'><input type='file' name='upload' style='height:35px; font-size:13px;' required>\r\n<input type='submit' value='Upload' class='button'></form>";
  temp += " </table><br>";
  server.sendContent(temp);
  temp = "";
  temp += "<td><a href =filesystem?format=on> Format SPIFFS Filesystem. (Takes up to 30 Seconds) </a></td>";
  temp += "<table border=2 bgcolor=white width=500 cellpadding=5><caption><p><h3>Systemlinks:</h2></p></caption><tr><th><br>";
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

void handleRoot(void)                           //  Main Page
{
 String temp = "";
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
  temp += "<!DOCTYPE HTML><html lang='de'><head><meta charset='UTF-8'><meta name= viewport content='width=device-width, initial-scale=1.0,'>";
  server.sendContent(temp);
  server.sendContent(css_definition);
  temp = "";
  temp += "<title>" PROJECT_TITLE "</title></head>";
  temp += "<h2>LED Display</h2>";
  temp += "<body>";
  server.sendContent(temp);
  temp = "";
// Processing User Request
if (server.args() > 0) // Parameter wurden ubergeben
{
  temp += "<br>Processing input. Please wait..<br><br>";
  server.sendContent(temp);
  temp = "";
  if(server.hasArg("PicSelect"))
  {
    if (server.arg("PicSelect") == "off")  // Clear Display
      {
        gfx_clearScreen();
        gfx_flushBuffer();
      }
    else
      {
        drawAnyImageType(server.arg("PicSelect").c_str()); // Bild gewählt. Display inhalt per Picselect hergstellt
      }
  }
}
  temp += "<table border=2 bgcolor = white ><caption><p><h3>Available Pictures in SPIFFS for "+String(gfx_getScreenWidth())+"*"+String(gfx_getScreenHeight())+" Display</h2></p></caption>";
  temp += "<form><tr><th><a href='?PicSelect=off&action=0'>Clear Display</a></th></tr>";
  server.sendContent(temp);
  //List available graphics files on SPIFFS
  ESP_CLASS_DIR root = esp_openDir("/");
  File file;
  PicCount = 1;
  while (file = esp_openNextFile(root))
  {
    bool valid = false;
    char *ext = strrchr(file.name(), '.');
    if(ext) ++ext;  // skip '.' itself, but not if not found!
    if(strcasecmp(ext, "bmp") == 0)
    {
      BMPHeader PicData = ReadBitmapSpecs(file.name());
      if (((PicData.width <= gfx_getScreenWidth()) && (PicData.height <= gfx_getScreenHeight())) && // Display only in list, when Bitmap not exceeding Display Resolution. Bigger Images are not listed. ...
          ((PicData.depth == 1) || (PicData.depth == 24)))                          // ... and when the bitmap has a known/understood bitdepth.
        {
          temp = String(PicData.width) + "*" + String(PicData.height) + "px*" + String(PicData.depth) + "bit";
          valid = true;
        }
    }
    else if ((strcasecmp(ext, "jpg") == 0) || (strcasecmp(ext, "jpeg") == 0))
    {
        if(JpegDec.decodeFsFile(file.name()))                                       // Display only in list, when file could be decoded ...
            if(JpegDec.width <= gfx_getScreenWidth() && JpegDec.height <= gfx_getScreenHeight())    // ... and does not exceed the display size
            {
                temp = String(JpegDec.width) + "*" + String(JpegDec.height) + "px";
                valid = true;
            }
    }
    if(valid)
    {
        temp = "<tr><th><label for='radio1'><img src='"+String(file.name())+"' alt='"+ String(file.name())+"' border='3' bordercolor=green> Image "+ PicCount+"</label><input type='radio' value='"+ String(file.name())+"' name='PicSelect'/><br> "+String(file.name())+": " + temp;
        temp += "; filesize: "+ formatBytes(file.size()) + "</th></tr>";
        server.sendContent(temp);
        if(PicCount <= SLIDESHOW_MAX_IMAGES)
        {
            strncpy(slideshow_filenames[PicCount-1], file.name(), MAX_FILENAME_LEN+1);
        }
        PicCount++;
    }
  }
  slideshow_num_images = PicCount - 1;
  temp = "<tr><th><button type='submit' name='action' value='0' style='height: 50px; width: 280px'>Show Image on Display</button></th></tr>";
  temp += "</form></table>";
  temp += "<br><table border=2 bgcolor = white width = 280 cellpadding =5 ><caption><p><h3>Systemlinks:</h2></p></caption>";
  temp += "<tr><td><br>";
  temp += "<a href='/settings'>Settings</a><br><br>";
  temp += "<a href='/filesystem'>Filemanager</a><br><br>";
  if(slideshow_num_images > 1)
  {
      temp += slideshow_is_running ? "<a href='/slideshow?off=1'>stop slideshow</a><br><br>" : "<a href='/slideshow?on=1'>start slideshow</a><br><br>";
  }
  temp += "<a href='/showwifi'>show WiFi info (like on startup; on the display)</a><br><br>";
  temp += "</td></tr></table><br><br>";
  temp += html_footer;
  temp += "</body></html>";
  server.sendContent(temp);
  temp = "";
  server.client().stop(); // Stop is needed because we sent no content length
}

// create/update the "index" of images that may be displayed - to be used in the slideshow
// in fact, this is a stripped down version of handleRoot()
// updates the global vars slideshow_filenames[] and slideshow_num_images
void scan_images_for_slideshow(void)
{
    File file;
    ESP_CLASS_DIR root = esp_openDir("/");

    slideshow_num_images = 0;
    while (file = esp_openNextFile(root))
    {
        bool valid = false;
        char *ext = strrchr(file.name(), '.');
        if(ext) ++ext;  // skip '.' itself, but not if not found!
        if(strcasecmp(ext, "bmp") == 0)
        {
            BMPHeader PicData = ReadBitmapSpecs(file.name());
            valid = ((PicData.width <= gfx_getScreenWidth()) && (PicData.height <= gfx_getScreenHeight())) && // Display only in list, when Bitmap not exceeding Display Resolution. Bigger Images are not listed. ...
                    ((PicData.depth == 1) || (PicData.depth == 24));                          // ... and when the bitmap has a known/understood bitdepth.
        }
        else if ((strcasecmp(ext, "jpg") == 0) || (strcasecmp(ext, "jpeg") == 0))
        {
            if(JpegDec.decodeFsFile(file.name()))                                       // Display only in list, when file could be decoded ...
                valid = (JpegDec.width <= gfx_getScreenWidth()) && (JpegDec.height <= gfx_getScreenHeight());    // ... and does not exceed the display size
        }
        if(valid && (slideshow_num_images < SLIDESHOW_MAX_IMAGES))
        {
            strncpy(slideshow_filenames[slideshow_num_images++], file.name(), MAX_FILENAME_LEN+1);
        }
    }
    if(slideshow_num_images < 1)    slideshow_is_running = false;
}

void handleNotFound(void)
{
    // If captive portal redirect instead of displaying the error page.
    if(captivePortal()) return;
  
    // if there is an according file: fine
    if (handleFileRead(server.urlDecode(server.uri()))) return;
    
    // else: send error page:
    String temp = "";
    // HTML Header
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    // HTML Content
    temp += "<!DOCTYPE HTML><html lang='de'><head><meta charset='UTF-8'><meta name= viewport content='width=device-width, initial-scale=1.0,'>";
    temp += css_definition;
    temp += "<title>" PROJECT_TITLE " - File not found</title></head>";
    temp += "<h2> 404 File Not Found</h2><br>";
    temp += "<h4>Debug Information:</h4><br>";
    temp += "<body>";
    temp += "URI: ";
    temp += server.uri();
    temp += "\nMethod: ";
    temp += ( server.method() == HTTP_GET ) ? "GET" : "POST";
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
    temp += "<a href='/settings'>Settings</a><br>";
    temp += "<a href='/filesystem'>Filemanager</a><br>";
    temp += "</th></tr></table><br><br>";
    temp += html_footer;
    temp += "</body></html>";
    server.send ( 404, "", temp );
    server.client().stop(); // Stop is needed because we sent no content length
}

// Redirect to captive portal if we got a request for another domain. Return true in that case so the page handler do not try to handle the request again.
boolean captivePortal(void)
{
  if (!isIp(server.hostHeader()) && server.hostHeader() != (String(ESPHostname)+".local")) {
    // Serial.println("Request redirected to captive portal");
    server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
    server.send ( 302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

// settings page handler
void handleSettings(void)
{
  //  page: /settings
  byte i, j, len;
  String temp = "";
  // check for site parameters

    // parameter save does not exist, if the page is just called, only when the form here was submitted
    // that check is necessary because there is no difference between an unchecked checkbox and a non-existing checkbox.
    // so, without the condition, all checkboxes would be like unchecked just before - when just entering this page.
    if(server.hasArg("save"))
    {
        // start slideshow automatically ?
        SETTINGS_PUT_SLIDESHOW_AUTORUN(server.hasArg("autorun_slideshow"));
        // show IP address on startup ?
        SETTINGS_PUT_SHOW_IP(server.hasArg("show_ip"));
        // show WiFi name (SSID) on startup ?
        SETTINGS_PUT_SHOW_SSID(server.hasArg("show_ssid"));
        // show WiFi (AP) password on startup ?
        SETTINGS_PUT_WIFI_PWD_EXHIBITION(server.hasArg("exhibit_passwd"));
    }

    if (server.hasArg("Reboot") )  // reboot system
       {
         temp = "Rebooting System in 5 Seconds..";
         server.send ( 200, "text/html", temp );
         delay(5000);
         server.client().stop();
         WiFi.disconnect();
         delay(1000);
         ESP.restart();
       }

    if (server.hasArg("WiFiMode") && (server.arg("WiFiMode") == "1")  )  // STA station mode connect to another WIFI station
       {
        // connect to existing STATION
        if ( sizeof(server.arg("WiFi_Network")) > 0  )
          {
            Serial.println("STA mode");
            SETTINGS_SET_STA_MODE;
            temp = "";
            for(i = 0; i < APSTANameLen; i++) MySettings.WiFiAPSTAName[i] = 0;
            temp = server.arg("WiFi_Network");
            len = temp.length();
            for(i = 0; i < len; i++) MySettings.WiFiAPSTAName[i] = temp[i];
            MySettings.WiFiAPSTAName[len+1] = 0;
            temp = "";

            for(i = 0; i < WiFiPwdLen; i++)  MySettings.WiFiPwd[i] = 0;
            temp = server.arg("STAWLanPW");
            len = temp.length();
            if(len > 0) // don't clear a previous password!
            {
                for( i = j = 0; i < len; i++)
                {
                    if (temp[i] > 32) // skip control chars
                    {
                        MySettings.WiFiPwd[j++] = temp[i];
                    }
                }
                MySettings.WiFiPwd[j] = 0;
            }
            temp = "WiFi connect to AP: '";
            temp += MySettings.WiFiAPSTAName;
            // temp += "'<br>WiFi PW: -";
            // temp += MySettings.WiFiPwd;
            temp += "'<br>";
            temp += "connecting to STA mode in 2 seconds..<br>";
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
            bool SaveOk = saveSettings();
            i = ConnectWifiAP();
            delay(700);
            if (i != 3) // 4: WL_CONNECT_FAILED - Password is incorrect 1: WL_NO_SSID_AVAILin - configured SSID cannot be reached
              {
                 Serial.print(F("cannot connect to specified network. reason: "));
                 Serial.println(i);
                 server.client().stop();
                 delay(100);
                 WiFi.setAutoReconnect (false);
                 delay(100);
                 WiFi.disconnect();
                 delay(1000);
                 SetDefaultWiFiSettings();
                 CreateWifiSoftAP();
                 return;
              } else
              {
                 // connection succeeded - save settings
                 bool SaveOk = saveSettings();
                 InitializeHTTPServer();
                 return;
              }
          }
       }

      if (server.hasArg("WiFiMode") && (server.arg("WiFiMode") == "2")  )  // change AP mode
       {
        // configure access point
        temp = server.arg("APPointName");
        len =  temp.length();
        temp = server.arg("APPW");
        i = server.hasArg("PasswordReq") ? temp.length() : 8;

        if (  ( len > 1 ) && (server.arg("APPW") == server.arg("APPWRepeat")) && ( i > 7) )
        {
            temp = "";
            Serial.println(F("APMode"));
            SETTINGS_SET_AP_MODE;
            SETTINGS_SET_PORTAL_CAPTIVITY(server.hasArg("CaptivePortal"));
            SETTINGS_PUT_WIFI_PWD_EXHIBITION(!server.hasArg("PasswordReq"));

            for (i = 0; i < APSTANameLen; i++)  MySettings.WiFiAPSTAName[i] = 0;
            temp = server.arg("APPointName");
            len = temp.length();
            for (i = 0; i < len; i++) MySettings.WiFiAPSTAName[i] = temp[i];
            MySettings.WiFiAPSTAName[len+1] = 0;
            temp = "";
            for (i = 0; i < WiFiPwdLen; i++)    MySettings.WiFiPwd[i] = 0;
            temp = server.arg("APPW");
            len = temp.length();
            for (i = 0; i < len; i++)   MySettings.WiFiPwd[i] = temp[i];
            MySettings.WiFiPwd[len+1] = 0;
            temp = "";
            temp = saveSettings() ? // save AP settings
                    "Settings saved successfully. Reboot required." :
                    "corrupted settings not saved.";
        } else temp = (server.arg("APPW") != server.arg("APPWRepeat")) ?
                  "WiFi password(s) differ. Aborted." :
                  "WiFi password too short. Aborted.";
       // End Wifi
       }

  // HTML Header
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
// HTML Content
  temp += "<!DOCTYPE HTML><html lang='de'><head><meta charset='UTF-8'><meta name=viewport content='width=device-width, initial-scale=1.0,'>";
  server.send ( 200, "text/html", temp );
  server.sendContent(css_definition);
  temp = "";
  temp += "<title>" PROJECT_TITLE " - Settings</title></head>";
  server.sendContent(temp);
  temp = "";
  temp += "<h2>WiFi Settings</h2><body><left>";
  temp += "<table border=2 bgcolor=white width=500><td><h4>Current WiFi Settings:</h4>";
  if (server.client().localIP() == apIP) {
     temp += "Mode : Soft Access Point (AP)<br>";
     temp += "SSID : " + String (MySettings.WiFiAPSTAName) + "<br><br>";
  } else {
     temp += "Mode : Station (STA) <br>";
     temp += "SSID  :  "+ String (MySettings.WiFiAPSTAName) + "<br>";
     temp += "BSSID :  " + WiFi.BSSIDstr()+ "<br><br>";
  }
  temp += "</td></table><br>";
  server.sendContent(temp);
  temp = "";
  temp += "<form action='/settings' method='post'><input type='hidden' name='save' value=1>";
  temp += "<table border=2 bgcolor = white width = 500><tr><th><br>";
  temp += SETTINGS_IS_AP_MODE ? "<input type='radio' value='1' name='WiFiMode' > WiFi Station Mode<br>" :
                                "<input type='radio' value='1' name='WiFiMode' checked > WiFi Station Mode<br>";
  temp += "Available WiFi Networks:<table border=2 bgcolor = white ></tr></th><td>Number </td><td>SSID  </td><td>Encryption </td><td>WiFi Strength </td>";
  server.sendContent(temp);
  temp = "";
  WiFi.scanDelete();
  int n = WiFi.scanNetworks(false, false); //WiFi.scanNetworks(async, show_hidden)
  if (n > 0)
  {
    for (int i = 0; i < n; i++)
    {
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
    temp += "<td>No WiFi found</td>";
    temp += "<td> --- </td>";
    temp += "<td> --- </td>";
  }
  temp += "</table><table border=2 bgcolor = white ></tr></th><td>Connect to WiFi SSID: </td><td><select name='WiFi_Network'>";
  if (n > 0) {
    for (int i = 0; i < n; i++) {
      temp += "<option value='" + WiFi.SSID(i) +"'";
      if(strcmp(WiFi.SSID(i).c_str(), MySettings.WiFiAPSTAName)==0)
        temp += " selected";
      temp += ">" + WiFi.SSID(i) +"</option>";
    }
  } else {
    temp += "<option value='No_WiFi_Network'>No WiFi network found !</option>";
  }
  server.sendContent(temp);
  temp = "";
  temp += "</select></td></tr></th><tr><td>WiFi Password: </td><td>";
  temp += "<input type='password' name='STAWLanPW' maxlength='40' size='40'>";
  temp += "</td></tr></th><br></th></tr></table></table><table border=2 bgcolor=white width=500><tr><th><br>";
  server.sendContent(temp);
  temp  = SETTINGS_IS_AP_MODE ? "<input type='radio' name='WiFiMode' value='2' checked> WiFi Access Point Mode<br>" :
                                "<input type='radio' name='WiFiMode' value='2' > WiFi Access Point Mode<br>";
  temp += "<table border=2 bgcolor = white ></tr></th> <td>WiFi Access Point Name: </td><td>";
  server.sendContent(temp);
  temp = SETTINGS_IS_AP_MODE ? "<input type='text' name='APPointName' maxlength='"+String(APSTANameLen-1)+"' size='30' value='" + String(MySettings.WiFiAPSTAName) + "'></td>" :
                               "<input type='text' name='APPointName' maxlength='"+String(APSTANameLen-1)+"' size='30' ></td>";
  server.sendContent(temp);
  temp = "";
  temp += "</tr></th><td>WiFi Password: </td><td>";
  if (SETTINGS_IS_AP_MODE)
    {
      temp += "<input type='password' name='APPW' maxlength='"+String(WiFiPwdLen-1)+"' size='30' value='" + String(MySettings.WiFiPwd) + "'> </td>";
      temp += "</tr></th><td>Repeat WiFi Password: </td>";
      temp += "<td><input type='password' name='APPWRepeat' maxlength='"+String(WiFiPwdLen-1)+"' size='30' value='" + String(MySettings.WiFiPwd) + "'> </td>";
    } else
    {
      temp += "<input type='password' name='APPW' maxlength='"+String(WiFiPwdLen-1)+"' size='30'> </td>";
      temp += "</tr></th><td>Repeat WiFi Password: </td>";
      temp += "<td><input type='password' name='APPWRepeat' maxlength='"+String(WiFiPwdLen-1)+"' size='30'> </td>";
    }
  temp += "</table>";
  server.sendContent(temp);
  temp = SETTINGS_IS_WIFI_PASSWORD_REQUIRED ? "<input type='checkbox' name='PasswordReq' checked> Password for Login required." :
                                              "<input type='checkbox' name='PasswordReq' > Password for Login required.";
  server.sendContent(temp);
  temp = SETTINGS_IS_CAPTIVE_PORTAL ? "<input type='checkbox' name='CaptivePortal' checked> Activate Captive Portal" :
                                      "<input type='checkbox' name='CaptivePortal' > Activate Captive Portal";
  temp += "<br></tr></th></table>";
  server.sendContent(temp);

  temp  = "<table border=2 bgcolor=white width=500 cellpadding=5><caption><h3>misc settings:</h3></caption><tr><th align=left><br>";
  temp += "<input type='checkbox' name='autorun_slideshow'";
  if(SETTINGS_IS_SLIDESHOW_AUTORUN)
      temp += " checked";
  temp += "> start slideshow automatically<br>";
  temp += "<input type='checkbox' name='show_ip'";
  if(SETTINGS_IS_IP_SHOWN)
      temp += " checked";
  temp += "> show IP address on startup<br>";
  temp += "<input type='checkbox' name='show_ssid'";
  if(SETTINGS_IS_SSID_SHOWN)
      temp += " checked";
  temp += "> show WiFi name (SSID) on startup<br>";
  temp += "<input type='checkbox' name='exhibit_passwd'";
  if(SETTINGS_IS_WIFI_PWD_EXHIBITED)
      temp += " checked";
  temp += "> show WiFi (AP) password on startup - <font color=red>unsafe!</font><br>";
  temp += "<br></th></tr></table>";
  server.sendContent(temp);

  temp = "";
  temp += "<br> <button type='submit' name='Settings' value='1' style='height: 50px; width: 140px' autofocus>Save Settings</button>";
  temp += "<button type='submit' name='Reboot' value='1' style='height: 50px; width: 200px' >Reboot System</button>";
  temp += "<button type='reset' name='action' value='1' style='height: 50px; width: 100px' >Reset</button></form>";
  server.sendContent(temp);

  temp  = "<table border=2 bgcolor=white width=500 cellpadding=5><caption><p><h3>Systemlinks:</h2></p></caption><tr><th><br>";
  temp += "<a href='/'>Main Page</a><br><br></th></tr></table><br><br>";
  temp += html_footer;
  temp += "</body></html>";
  server.sendContent(temp);
  server.client().stop(); // Stop is needed because we sent no content length
  temp = "";
}

void handleUploadSave(void)
{
  // String FileData;
  String temp = "";
  for (byte i = 0; i < server.args(); i++)
  {
    temp += "Arg " + (String)i + " –> ";   //Include the current iteration value
    temp += server.argName(i) + ": ";     //Get the name of the parameter
    temp += server.arg(i) + "\n";              //Get the value of the parameter
  }
  // server.send(200, "text/plain", temp);       //Response to the HTTP request
  // FileData = server.arg("datei");
  server.sendHeader("Location", "filesystem", true);
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send ( 302, "text/plain", "");  // Empty content inhibits Content-length header so we have to close the socket ourselves.
  server.client().stop(); // Stop is needed because we sent no content length
}

void handleSlideshow(void)
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

void doShowWifi(bool force)
{
    gfx_clearScreen();

    // ip address
    { IPAddress myIP = WiFi.localIP();
    if((uint32_t)myIP == 0)
    {
        myIP = WiFi.softAPIP();
        Serial.print("WiFi.softAPIP() = ");
    }
    else
        Serial.print("WiFi.localIP() = ");
    Serial.println(myIP);
    // gfx_setTextColor(1); // never changed after setup(), so not necessary
    if(force || SETTINGS_IS_IP_SHOWN)   gfx_drawString(10,12, myIP.toString().c_str());
    }

    // SSID
    Serial.println("WiFi.SSID = " + WiFi.SSID());
    if(force || SETTINGS_IS_SSID_SHOWN) gfx_drawString(10,32, WiFi.SSID().c_str());

    // AP password
    if(SETTINGS_IS_AP_MODE)
    {
        if(SETTINGS_IS_WIFI_PASSWORD_REQUIRED)
        {
            Serial.print("WiFi(AP) password = ");
            Serial.println(MySettings.WiFiPwd);
            if(SETTINGS_IS_WIFI_PWD_EXHIBITED)  gfx_drawString(10,52, MySettings.WiFiPwd);
        }
        else
        {
            Serial.println("'open' WiFi AP: unencrypted, unsafe, no password required");
            if(SETTINGS_IS_WIFI_PWD_EXHIBITED)  gfx_drawString(10,52, "no pw required");
        }
    }

    gfx_flushBuffer();
}

void handleShowWifi(void)
{
    doShowWifi(true);
    // "suspend" the slideshow for one cycle (no need to check if it is running)
    slideshow_last_switch = millis();

    // "redirect" to main page
    server.sendHeader("Location", "/", true);
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send ( 302, "text/plain", "");  // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop(); // Stop is needed because we sent no content length
}

// Is this an IP?
boolean isIp(String str)
{
  for (int i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) return false;
  }
  return true;
}

String GetEncryptionType(byte thisType)
{
  String Output = "";
   // read the encryption type and print out the name:
   switch (thisType) {
     case 2: return String("WPA");  break;
     case 4: return String("WPA2"); break;
     case 5: return String("WEP");  break;
     case 7: return String("None"); break;
     case 8: return String("Auto"); break;
   }
}

// convert IPAddress to String
String toStringIp(IPAddress ip)
{
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

String formatBytes(size_t bytes)    // create human readable versions of memory amounts
{
   if (bytes < 1024)                return String(bytes) + " Bytes";
   else if (bytes < (1024 * 1024))  return String(bytes / 1024.0) + " KB";
   else                             return String(bytes / 1024.0 / 1024.0) + " MB";
}

String getContentType(String filename)      // convert the file extension to the MIME type
{
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

bool handleFileRead(String path)    // send the right file to the client (if it exists)
{
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

void InitializeHTTPServer(void)
 {
  bool initok = false;
  /* Setup web pages: root, wifi settings pages, SO captive portal detectors and not found. */
  server.on("/", handleRoot);
  server.on("/settings", handleSettings);
  server.on("/filesystem", HTTP_GET, handleDisplayFS);
  server.on("/slideshow", HTTP_GET, handleSlideshow);
  server.on("/showwifi", HTTP_GET, handleShowWifi);
  server.on("/upload", HTTP_POST, []() {
  server.send(200, "text/plain", "");
  }, handleFileUpload);
  if(SETTINGS_IS_CAPTIVE_PORTAL)
  {
    server.on("/generate_204", handleRoot);     //Android captive portal. Maybe not needed. Might be handled by notFound handler.
    server.on("/favicon.ico", handleRoot);      //Another Android captive portal. Maybe not needed. Might be handled by notFound handler. Checked on Sony Handy
    server.on("/fwlink", handleRoot);           //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
  }
  server.onNotFound ( handleNotFound );
  server.begin(); // Web server start
 }

