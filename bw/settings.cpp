/*

Tobis General Display

by Arnold Schommer

settings.cpp - functions for handling the persistent settings, saved in EEPROM

*/

#include <Arduino.h>
#include <string.h>
#include "settings.h"

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
bool loadSettings(void)
{
    bool RetValue;
    EEPROM.begin(512);
    EEPROM.get(0, MySettings);
    EEPROM.end();
    // debug_print_settings();
    return strcmp(MySettings.SettingsValid, MAGIC_VALUE_SETTINGS_VALID) == 0;
}

// store settings data in EEPROM
bool saveSettings(void)
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
void SetDefaultWiFiSettings(void)
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
void SetDefaultSettings(void)
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
