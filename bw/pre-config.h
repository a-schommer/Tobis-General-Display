/*

Tobis General Display

pre-config.h - some defines to be used within config.h
by Arnold Schommer

please do not change this, just change config.h

*/

#ifndef _PRE_CONFIG_H
#define _PRE_CONFIG_H

// hostname (for mdns)
#ifdef ESP32
#define FALLBACK_HOSTNAME   "ESP32"
#elif defined(ESP8266)
#define FALLBACK_HOSTNAME   "ESP8266"
#else
#define FALLBACK_HOSTNAME   "ESP"
#endif

#endif _PRE_CONFIG_H
