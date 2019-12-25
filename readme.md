# Tobi's General Display

[tocstart]: # (toc start)

  * [Introduction](#introduction)
  * [Requirements](#requirements)
  * [Setting up](#setting-up)
  * [State & ideas](#Current-state-and-ideas-for-the-future)
  * [Project Origin](#The-origin-of-this-project)
  * [License](#license)

[tocend]: # (toc end)

## Introduction

These little sketches enables you to simply show tiny images on small OLEDs (or LCDs) attached to an ESP32 device, controlling the whole via WiFi.
It is less meant as something to use than as something to get ideas from. 
You can upload images to the ESP32, select which of them is displayed and even run a slideshow - all by a simple web frontend.
The name is still (almost) the same as in the project this is forked from - see [Project Origin](#The-origin-of-this-project).

## Requirements

What do you need to "use" this sketch?

* Some ESP32 driven device

* Any device supported by u8g2 or ucglib attachecd to your ESP32
  And, especially if it is "on board": The knowledge how to set it up using u8g2; basically: what driver chip does it have, is it I2C or SPI attached, what pins are used.

* The Arduino IDE
  (I simply do not know about the ESP IDF)

* The ESP32 Board Manager (https://github.com/espressif/arduino-esp32)
  There are others and i would expect them to work, too - but i simply did not check that. 
  Bottom line: If you get strange compilation issues, give that one a try.
  
* If you use a black&white only display: the u8g2 library from olikraus, https://github.com/olikraus/u8g2/wiki
  If you use a color display: the ucglib library from olikraus, https://github.com/olikraus/ucglib

* The JPEGDecoder library from Bodmer, https://github.com/Bodmer/JPEGDecoder

## Setting Up

There are two separate, but quite similair versions: bw for black&white displays, using u8g2 and color for color displays, using ucglib.
Take a look at config.h - this should contain everything you need to adapt it to your concrete hardware.

## Current state and ideas for the future

You can already
* use it as a stand-alone Wifi "AP" or configure it as a station in your WiFi of choice
* upload files to the SPIFFS filesystem on the ESP32 via WiFi (and delete them)
* select an image from a list to be displayed on an OLED
* use any device supported by the u8g2 or ucglib library
* display Windows Bitmap Files (of depth 1bit = black&white, non-compressed or 24bit)
* display JPEG files (non progressive, as Bodmer's JPEGDecoder library "demands", too)

Ideas for the future
I do not really plan to do all of this; feel free to realize them as forks:
* (fork:) use an SD card instead of SPIFFS.
  In fact i assume this makes little sense. I already see the webserver of an ESP32 becoming unstable with about 20 files being listed... imagine, what might happen with an SD card full of images.
* making it work on ESP8266

## The origin of this project
This project started as a fork of [Captive_Portal](https://github.com/KuchTo/Captive_Portal) by Tobias Kuch, but just part Captive Portal_ESP32_LED_Matrix.
Well, more or less: in fact it started before that was uploaded to GitHub (based on a prior release on a German blog).
The graphics part of course changed a lot due to the different output devices, but the frame part (originally) was almost unchanged. Later, i changed quite a lot of the HTML output.

The JPEG display routines (JPEG_functions.ino) are taken from the JPEGDecoder examples which is MIT-licensed.
(I adapted that from the Adafruit_GFX for NodeMCU to be more precise, but i guess that no more matters)

## License

This project is licensed under the GNU General Public License, version 3 - see [LICENSE](./LICENSE).

JPEG_functions.ino: derived from JPEGDecoder library examples; this library has the MIT license, see file [JPEGDecoder-LICENSE](./JPEGDecoder-LICENSE).
