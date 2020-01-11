/*

Tobis General Display
by Arnold Schommer, Tobias Kuch

network.h - network (WiFi, HTTP-UI) related stuff, declaration

*/

#ifndef NETWORK_H
#define NETWORK_H

#include "pre-config.h"
#include "config.h"
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
#include <JPEGDecoder.h>    // https://github.com/Bodmer/JPEGDecoder

extern const char *ESPHostname;     // hostname for mDNS. Should work at least on windows. Try http://<hostname>.local

extern DNSServer dnsServer;         // DNS server
extern WEBSERVER_CLASS server;      // Web server

// Conmmon Paramenters
extern bool SoftAccOK;

// buffer of filenames for the pictures available for the slideshow
extern int slideshow_num_images;
extern char slideshow_filenames[SLIDESHOW_MAX_IMAGES][MAX_FILENAME_LEN+1];

void InitializeHTTPServer(void);
boolean CreateWifiSoftAP(void);
byte ConnectWifiAP(void);

char *urlencode(char const *from);      // mask special characters, returning a pseudo-copy - in fact, to a static buffer...

// create/update the "index" of images that may be displayed - to be used in the slideshow
// in fact, this is a stripped down version of handleRoot()
// updates the global vars slideshow_filenames[] and slideshow_num_images
void scan_images_for_slideshow(void);

void handleFileUpload(void);            // upload a new file to the SPIFFS
void handleDisplayFS(void);
void handleRoot(void);
void handleNotFound(void);
void handleSettings(void);              // settings page handler
void handleUploadSave(void);
void handleSlideshow(void);
void handleShowWifi(void);
bool handleFileRead(String path);       // send the right file to the client (if it exists)

boolean captivePortal(void);            // Redirect to captive portal if we got a request for another domain. Return true in that case so the page handler do not try to handle the request again.

void doShowWifi(bool force);

boolean isIp(String str);           // Is this an IP?
String toStringIp(IPAddress ip);    // IP to String conversion
String GetEncryptionType(byte thisType);
String formatBytes(size_t bytes);   // create human readable versions of memory amounts
String getContentType(String filename); // convert the file extension to the MIME type

#endif NETWORK_H
