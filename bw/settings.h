/*

Tobis General Display

by Arnold Schommer

settings.h - definitions for the settings to be saved in EEPROM

*/

#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <EEPROM.h>
#include "config.h"

// flag-"bits" for the flags-field of the EEPROM-Data, struct EEPromData
// try to start in station mode? If not, we already start in AP mode (which will act as fallback, too, if the WiFi for station mode is unavailabel)
#define SETTINGS_WIFI_AP_MODE                   1
// does the AP encrypt (using WPA)? If so, a password is required, too
#define SETTINGS_WIFI_ENCRYPTED                 2
// (only relevant in AP mode:) does AP provide a *captive* portal?
#define SETTINGS_WIFI_CAPTIVE_PORTAL            4
// shall the slideshow run automatically?
#define SETTINGS_AUTORUN_SLIDESHOW              8
// show the IP address on the startup screen?
#define SETTINGS_STARTUP_SHOW_IP                16
// show the SSID on the startup screen?
#define SETTINGS_STARTUP_SHOW_SSID              32
// show the AP password on the startup screen? (only, if in AP mode)
#define SETTINGS_STARTUP_EXHIBIT_WIFI_PASSWORD  64

// access macros to test/set/clear those flags:
// generic:
#define SETTINGS_TEST(flag)             ((MySettings.flags & (flag)) != 0)
#define SETTINGS_SET_FLAG(flag)         (MySettings.flags |=  (flag))
#define SETTINGS_CLEAR_FLAG(flag)       (MySettings.flags &= ~(flag))
#define SETTINGS_PUT_FLAG(flag, state)  ((state) ? SETTINGS_SET_FLAG(flag) : SETTINGS_CLEAR_FLAG(flag))
// set AP/STA mode:
#define SETTINGS_IS_AP_MODE                     SETTINGS_TEST(SETTINGS_WIFI_AP_MODE)
#define SETTINGS_IS_STA_MODE                    (!SETTINGS_TEST(SETTINGS_WIFI_AP_MODE))
#define SETTINGS_SET_AP_MODE                    SETTINGS_SET_FLAG(SETTINGS_WIFI_AP_MODE)
#define SETTINGS_SET_STA_MODE                   SETTINGS_CLEAR_FLAG(SETTINGS_WIFI_AP_MODE)
// encryption
#define SETTINGS_IS_WIFI_ENCRYPTED              SETTINGS_TEST(SETTINGS_WIFI_ENCRYPTED)
#define SETTINGS_IS_WIFI_PASSWORD_REQUIRED      SETTINGS_IS_WIFI_ENCRYPTED
#define SETTINGS_SET_WIFI_UNENCRYPTION(state)   SETTINGS_PUT_FLAG(SETTINGS_WIFI_ENCRYPTED, state)
#define SETTINGS_SET_WIFI_ENCRYPTED             SETTINGS_SET_FLAG(SETTINGS_WIFI_ENCRYPTED)
#define SETTINGS_SET_WIFI_UNENCRYPTED           SETTINGS_CLEAR_FLAG(SETTINGS_WIFI_ENCRYPTED)
// Captive Portal
#define SETTINGS_IS_CAPTIVE_PORTAL              SETTINGS_TEST(SETTINGS_WIFI_CAPTIVE_PORTAL)
#define SETTINGS_SET_PORTAL_CAPTIVITY(state)    SETTINGS_PUT_FLAG(SETTINGS_WIFI_CAPTIVE_PORTAL, state)
#define SETTINGS_SET_CAPTIVE_PORTAL             SETTINGS_SET_FLAG(SETTINGS_WIFI_CAPTIVE_PORTAL)
#define SETTINGS_UNSET_CAPTIVE_PORTAL           SETTINGS_CLEAR_FLAG(SETTINGS_WIFI_CAPTIVE_PORTAL)
// slideshow automatic run?
#define SETTINGS_IS_SLIDESHOW_AUTORUN           SETTINGS_TEST(SETTINGS_AUTORUN_SLIDESHOW)
#define SETTINGS_PUT_SLIDESHOW_AUTORUN(state)   SETTINGS_PUT_FLAG(SETTINGS_AUTORUN_SLIDESHOW, state)
#define SETTINGS_SET_SLIDESHOW_AUTORUN          SETTINGS_SET_FLAG(SETTINGS_AUTORUN_SLIDESHOW)
#define SETTINGS_UNSET_SLIDESHOW_AUTORUN        SETTINGS_CLEAR_FLAG(SETTINGS_AUTORUN_SLIDESHOW)
// show the IP address on the startup screen?
#define SETTINGS_IS_IP_SHOWN                    SETTINGS_TEST(SETTINGS_STARTUP_SHOW_IP)
#define SETTINGS_PUT_SHOW_IP(state)             SETTINGS_PUT_FLAG(SETTINGS_STARTUP_SHOW_IP, state)
#define SETTINGS_SET_SHOW_IP                    SETTINGS_SET_FLAG(SETTINGS_STARTUP_SHOW_IP)
#define SETTINGS_UNSET_SHOW_IP                  SETTINGS_CLEAR_FLAG(SETTINGS_STARTUP_SHOW_IP)
// show the SSID on the startup screen?
#define SETTINGS_IS_SSID_SHOWN                  SETTINGS_TEST(SETTINGS_STARTUP_SHOW_SSID)
#define SETTINGS_PUT_SHOW_SSID(state)           SETTINGS_PUT_FLAG(SETTINGS_STARTUP_SHOW_SSID, state)
#define SETTINGS_SET_SHOW_SSID                  SETTINGS_SET_FLAG(SETTINGS_STARTUP_SHOW_SSID)
#define SETTINGS_UNSET_SHOW_SSID                SETTINGS_CLEAR_FLAG(SETTINGS_STARTUP_SHOW_SSID)
// show the AP password (not: WiFi STA password) on the startup screen?
#define SETTINGS_IS_WIFI_PWD_EXHIBITED          SETTINGS_TEST(SETTINGS_STARTUP_EXHIBIT_WIFI_PASSWORD)
#define SETTINGS_PUT_WIFI_PWD_EXHIBITION(state) SETTINGS_PUT_FLAG(SETTINGS_STARTUP_EXHIBIT_WIFI_PASSWORD, state)
#define SETTINGS_SET_WIFI_PWD_EXHIBITED         SETTINGS_SET_FLAG(SETTINGS_STARTUP_EXHIBIT_WIFI_PASSWORD)
#define SETTINGS_UNSET_WIFI_PWD_EXHIBITED       SETTINGS_CLEAR_FLAG(SETTINGS_STARTUP_EXHIBIT_WIFI_PASSWORD)

static const char WiFiPwdLen = WIFIPWDLEN;
static const char APSTANameLen = APSTANAMELEN;

// magic value for a naive check if the settings structure is valid
#define MAGIC_VALUE_SETTINGS_VALID "TKAS"   // must be less than five bytes!!!

struct EEPromData
  {
    uint16_t flags;
    char WiFiAPSTAName[APSTANameLen];   // STATION /AP name to connect, if definded
    char WiFiPwd[WiFiPwdLen];           // WiFiPAssword, if definded
    char SettingsValid[5];              // magic value for naive check of validity of the data structure
  };

extern struct EEPromData MySettings;

void debug_print_settings(void);    // print the settings data human readable only on Serial, for debugging purposes
bool loadSettings(void);            // load settings data from EEPROM
bool saveSettings(void);            // store settings data in EEPROM
void SetDefaultWiFiSettings(void);  // initialize only the WiFi part of the settings data
void SetDefaultSettings(void);      // initialize the whole settings data

#endif _SETTINGS_H
