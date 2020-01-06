/*

Tobis General Display

gfxlayer.h
by Arnold Schommer

graphics relevant "interfaces" - mostly defined as inline functions or macros.

u8g2 variant

*/

#ifndef GFXLAYER_H
#define GFXLAYER_H

inline void gfx_flushBuffer(void)                               { u8g2.sendBuffer(); }  // write the whole buffer to the display - only needed if the gfx system uses a framebuffer instead of writing everything to the display immediately
inline void gfx_clearScreen(void)                               { u8g2.clearDisplay(); }
inline uint16_t gfx_getScreenWidth(void)                        { return u8g2.getDisplayWidth(); }
inline uint16_t gfx_getScreenHeight(void)                       { return u8g2.getDisplayHeight(); }
inline void gfx_setPixel(uint16_t x, uint16_t y)                { u8g2.drawPixel(x, y); }
inline void gfx_setPixelColor(uint8_t c)                        { u8g2.setDrawColor(c); }
inline void gfx_setTextColor(uint8_t c)                         { u8g2.setDrawColor(c); }
inline void gfx_drawString(uint16_t x, uint16_t y, const char *str)   { u8g2.drawStr(x,y, str); }

inline void gfx_init(void)  // calls gfx_clearScreen() and gfx_flushBuffer(); therefore defined afterwards
{
    u8g2.begin();
#ifdef BRIGHTNESS
    u8g2.setContrast(BRIGHTNESS);
#endif
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_ncenR10_tr);
    gfx_clearScreen();
    gfx_flushBuffer();
}

#endif GFXLAYER_H
