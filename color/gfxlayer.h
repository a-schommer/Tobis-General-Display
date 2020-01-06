/*

Tobis General Display

gfxlayer.h
by Arnold Schommer

graphics relevant "interfaces" - mostly defined as inline functions or macros.

ucg variant

*/

#ifndef GFXLAYER_H
#define GFXLAYER_H

inline void gfx_flushBuffer(void)                               { }     // write the whole buffer to the display - only needed if the gfx system uses a framebuffer instead of writing everything to the display immediately
inline void gfx_clearScreen(void)                               { ucg.clearScreen(); }
inline uint16_t gfx_getScreenWidth(void)                        { return ucg.getWidth(); }
inline uint16_t gfx_getScreenHeight(void)                       { return ucg.getHeight(); }
inline void gfx_setPixel(uint16_t x, uint16_t y)                { ucg.drawPixel(x, y); }
inline void gfx_setPixelColor(uint8_t r, uint8_t g, uint8_t b)  { ucg.setColor(r, g, b); }
inline void gfx_setTextColor(uint8_t r, uint8_t g, uint8_t b)   { ucg.setColor(r, g, b); }
inline void gfx_drawString(uint16_t x, uint16_t y, const char *str)   { ucg.drawString(x,y, 0, str); }

inline void gfx_init(void)  // calls gfx_clearScreen() and gfx_flushBuffer(); therefore defined afterwards
{
    ucg.begin(UCG_FONT_MODE_TRANSPARENT);
    ucg.setFont(ucg_font_ncenR10_tr);
    ucg.setFontMode(UCG_FONT_MODE_TRANSPARENT);
    gfx_clearScreen();
    gfx_flushBuffer();
}

#endif GFXLAYER_H
