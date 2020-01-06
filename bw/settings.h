/*

Tobis General Display

by Arnold Schommer

settings.h - definitions concerning the settings to be saved in EEPROM

CAUTION: also contains few functions and global variables!

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

static const byte WiFiPwdLen = WIFIPWDLEN;
static const byte APSTANameLen = APSTANAMELEN;

// magic value for a naive check if the settings structure is valid
#define MAGIC_VALUE_SETTINGS_VALID "TKAS"   // must be less than five bytes!!!

struct EEPromData
  {
    uint16_t flags;
    char WiFiAPSTAName[APSTANameLen];   // STATION /AP name to connect, if definded
    char WiFiPwd[WiFiPwdLen];           // WiFiPAssword, if definded
    char SettingsValid[5];              // magic value for naive check of validity of the data structure
  };

struct EEPromData MySettings;

// print the settings data human readable only on Serial, for debugging purposes:
void debug_print_settings(void)
{   char buffer[20];

    Serial.println(F("\ndebug_print_settings()"));
    Serial.print("flags\t$");
    Serial.print(itoa(MySettings.flags, buffer, 16));
    if(MySettings.flags)
    {   Serial.print(" = ");
        Serial.print((MySettings.flags & SETTINGS_WIFI_AP_MODE) ? "AP Mode":"STA Mode");
        Serial.print((MySettings.flags & SETTINGS_WIFI_ENCRYPTED) ? " WIFI_ENCRYPTED":" open WiFi");
        if(SETTINGS_IS_AP_MODE && (MySettings.flags & SETTINGS_WIFI_CAPTIVE_PORTAL))
            Serial.print(" WIFI_CAPTIVE_PORTAL");
        if(MySettings.flags & SETTINGS_AUTORUN_SLIDESHOW)
            Serial.print(" AUTORUN_SLIDESHOW");
        if(MySettings.flags & SETTINGS_STARTUP_SHOW_IP)
            Serial.print(" STARTUP_SHOW_IP");
        if(MySettings.flags & SETTINGS_STARTUP_SHOW_SSID)
            Serial.print(" STARTUP_SHOW_SSID");
        if(SETTINGS_IS_AP_MODE && (MySettings.flags & SETTINGS_STARTUP_EXHIBIT_WIFI_PASSWORD))
            Serial.print(" STARTUP_EXHIBIT_WIFI_PASSWORD");
        Serial.print("\n");
    }
    Serial.print("WiFiAPSTAName\t");
    Serial.println(MySettings.WiFiAPSTAName[0] ? MySettings.WiFiAPSTAName : "(unset)");
    Serial.print("WiFiPwd\t");
    Serial.println(MySettings.WiFiPwd[0] ? "(set)" : "(unset)");
    Serial.print("SettingsValid\t");
    Serial.print(MySettings.SettingsValid);
    Serial.println((strcmp(MySettings.SettingsValid, MAGIC_VALUE_SETTINGS_VALID) == 0) ? " - valid":" - invalid!");
}

// load settings data from EEPROM
bool loadSettings()
{
    bool RetValue;
    EEPROM.begin(512);
    EEPROM.get(0, MySettings);
    EEPROM.end();
    // debug_print_settings();
    return strcmp(MySettings.SettingsValid, MAGIC_VALUE_SETTINGS_VALID) == 0;
}

// store settings data in EEPROM
bool saveSettings()
{
    bool RetValue = true;

    // Check logical Errors
    if(SETTINGS_IS_AP_MODE)
    {
        if(SETTINGS_IS_WIFI_PASSWORD_REQUIRED && (sizeof(String(MySettings.WiFiPwd)) < 8))
            RetValue = false;  // invalid settings
        if(sizeof(String(MySettings.WiFiAPSTAName)) < 1)
            RetValue = false;  // invalid settings
    }
    if (RetValue)
    {
        EEPROM.begin(512);
        for (int i = 0 ; i < sizeof(MySettings) ; i++)
            EEPROM.write(i, 0);
        strncpy( MySettings.SettingsValid, MAGIC_VALUE_SETTINGS_VALID, sizeof(MySettings.SettingsValid) );
        EEPROM.put(0, MySettings);
        EEPROM.commit();
        EEPROM.end();
        // debug_print_settings();
    }
    return RetValue;
}

// initialize only the WiFi part of the settings data
void SetDefaultWiFiSettings()
{
   // AP mode, encrypted (==password required), captive protal:
    SETTINGS_SET_AP_MODE;
    SETTINGS_SET_WIFI_ENCRYPTED;
    SETTINGS_SET_CAPTIVE_PORTAL;

    strncpy( MySettings.WiFiAPSTAName, FALLBACK_APSTANAME, sizeof(MySettings.WiFiAPSTAName) );
    MySettings.WiFiAPSTAName[strlen(FALLBACK_APSTANAME)+1] = '\0';
    strncpy( MySettings.WiFiPwd, FALLBACK_WIFIPWD, sizeof(MySettings.WiFiPwd) );
    MySettings.WiFiPwd[strlen(FALLBACK_WIFIPWD)+1] = '\0';
    Serial.println(F("WiFi settings reset."));
}

// initialize the whole settings data
void SetDefaultSettings()
{
    SetDefaultWiFiSettings();

    // Flags (on top of WiFi related): show IP & SSID on startup
    SETTINGS_UNSET_SLIDESHOW_AUTORUN;
    SETTINGS_SET_SHOW_IP;
    SETTINGS_SET_SHOW_SSID;
    SETTINGS_UNSET_WIFI_PWD_EXHIBITED;

    strncpy( MySettings.SettingsValid, MAGIC_VALUE_SETTINGS_VALID, sizeof(MySettings.SettingsValid) );
    MySettings.SettingsValid[strlen(MAGIC_VALUE_SETTINGS_VALID)+1] = '\0';
    Serial.println(F("All settings reset."));
}

#endif _SETTINGS_H
