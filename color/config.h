/*

Tobis General Display

config.h - "user" configuration
by Arnold Schommer

*/

#ifndef _CONFIG_H
#define _CONFIG_H

#define PROJECT_TITLE   "Tobi's General Display"
// for "debug" output on serial: baudrate to set up
#define BAUDS 115200

// this define defines a complete declaration&definition statement (excluding the final ;) 
// to construct some u8g2/ucg object specific to the hardware being used:
// the object name *must* be u8g2/ucg (as lots of source relies on this)
// GuoYun + SSD1351:
#define UCG_CONSTRUCTION Ucglib_SSD1351_18x128x128_HWSPI ucg(/*cd=*/ 17, /*cs=*/ 21, /*reset=*/ 16)

// name & password of the wifi to create if we can't join any:
#define FALLBACK_APSTANAME  "ESP_Config"
#define FALLBACK_WIFIPWD    "EspWiFiDisplay"
// hostname (for mdns)
// optional: if not set here, it will be "ESP32" or "ESP8266" depending on the DEFINEs to the compiler
// #define FALLBACK_HOSTNAME   "ESP_Tobi"

// max. length of wifi passwords (to save)
#define WIFIPWDLEN  25
// max. length of wifi nets (to save)
#define APSTANAMELEN 20

// how long (ms) should each frame be showed during the slideshow?
#define SLIDESHOW_PERIOD 3000

// (max.) buffer slots for images:
#define SLIDESHOW_MAX_IMAGES    64
// (max.) filename length (i did not find a define for how long an SPIFFS filename may be); longer filenames will be cut to this!
#define MAX_FILENAME_LEN        32


#endif _CONFIG_H
