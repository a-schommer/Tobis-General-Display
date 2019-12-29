/*

Tobis General Display

esplayer.h

"interfaces" which differ between esp8266 and esp32 - mostly defined as inline functions or macros.
contains several #include's !

*/

#ifndef ESPLAYER_H
#define ESPLAYER_H

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#define FS_NO_GLOBALS       // required ba JPEGDecoder
#include <FS.h>

#define WEBSERVER_CLASS     ESP8266WebServer

inline void esp_guru_meditation_error_remediation(void) {}  // that is ESP32 specific
inline void esp_wifi_set_hostname(const char *name) { WiFi.hostname(name); }
inline size_t esp_get_fs_usedBytes(void)  { struct FSInfo fsi; return SPIFFS.info(fsi) ? fsi.usedBytes  : -1; }
inline size_t esp_get_fs_totalBytes(void) { struct FSInfo fsi; return SPIFFS.info(fsi) ? fsi.totalBytes : -1; }

// directory scanning works quite different for ESP8266 and ESP32...
#define ESP_CLASS_DIR   Dir
inline Dir esp_openDir(const char* path) { return SPIFFS.openDir(path); }
inline File esp_openNextFile(Dir dir) { do dir.next(); while(dir.isDirectory()); return dir.openFile("r"); }

#endif

#ifdef ESP32
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#define FS_NO_GLOBALS       // required ba JPEGDecoder
#include <SPIFFS.h>

#define WEBSERVER_CLASS     WebServer

#define GPIO_OUT_W1TS_REG (DR_REG_GPIO_BASE + 0x0008)
#define GPIO_OUT_W1TC_REG (DR_REG_GPIO_BASE + 0x000c)

inline void esp_guru_meditation_error_remediation(void)
{
  REG_WRITE(GPIO_OUT_W1TS_REG, BIT(GPIO_NUM_16));     // Guru Meditation Error Remediation set
  delay(1);
  REG_WRITE(GPIO_OUT_W1TC_REG, BIT(GPIO_NUM_16));     // Guru Meditation Error Remediation clear
}

inline void esp_wifi_set_hostname(const char *name) { WiFi.setHostname(name); }
inline size_t esp_get_fs_usedBytes(void)  { return SPIFFS.usedBytes(); }
inline size_t esp_get_fs_totalBytes(void) { return SPIFFS.totalBytes(); }

// directory scanning works quite different for ESP8266 and ESP32...
#define ESP_CLASS_DIR   File
inline fs::File esp_openDir(const char* path) { return SPIFFS.open(path); }
inline fs::File esp_openNextFile(fs::File dir) { return dir.openNextFile(); }

#endif

#endif ESPLAYER_H
