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
// GuoYun & SH1106:
#define U8G2_CONSTRUCTION U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /*reset= */ U8X8_PIN_NONE, /* clock= */ 14, /* data= */ 15)
// Heltec WifiKit 32 & internal SSD1306:
// #define U8G2_CONSTRUCTION U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /*reset= */ 16, /* clock= */ 15, /* data= */ 4)
// some WeMos (?) board with onboard-SSD1306 & battery holder  (18650 size)
// #define U8G2_CONSTRUCTION U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(/* u8g2_cb_t *rotation = */ U8G2_R0, /* reset = */ U8X8_PIN_NONE, /* SCL= */ 4, /* SDA= */ 5 )

// optional(!): brightness to set
// #define BRIGHTNESS 100

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
