#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>

#define ST7789_WIDTH  240
#define ST7789_HEIGHT 240

/* Font scales */
#define FONT_SMALL  1   /* 8x8 base font */
#define FONT_MEDIUM 2   /* 16x16 */
#define FONT_LARGE  3   /* 24x24 */

/* RGB565 color macro (produces big-endian uint16_t for SPI) */
#define RGB565(r, g, b) \
    __builtin_bswap16((uint16_t)(((r) & 0xF8) << 8) | \
                                (((g) & 0xFC) << 3) | \
                                ((b) >> 3))

/* Predefined colors */
#define COLOR_BLACK   RGB565(0, 0, 0)
#define COLOR_WHITE   RGB565(255, 255, 255)
#define COLOR_RED     RGB565(255, 0, 0)
#define COLOR_GREEN   RGB565(0, 255, 0)
#define COLOR_BLUE    RGB565(0, 0, 255)
#define COLOR_YELLOW  RGB565(255, 255, 0)
#define COLOR_CYAN    RGB565(0, 255, 255)
#define COLOR_MAGENTA RGB565(255, 0, 255)
#define COLOR_GRAY    RGB565(128, 128, 128)
#define COLOR_DARK_GRAY RGB565(60, 60, 60)

/* Init / close */
int  st7789_init(const char *spi_device, int dc_pin, int rst_pin);
void st7789_close(void);

/* Framebuffer drawing (compose, then flush) */
void st7789_clear(uint16_t color);
void st7789_pixel(int x, int y, uint16_t color);
void st7789_hline(int x, int y, int w, uint16_t color);
void st7789_vline(int x, int y, int h, uint16_t color);
void st7789_fill_rect(int x, int y, int w, int h, uint16_t color);
void st7789_draw_char(int x, int y, char c, uint16_t color, int scale);
void st7789_draw_string(int x, int y, const char *str, uint16_t color, int scale);
int  st7789_string_width(const char *str, int scale);
void st7789_flush(void);

#endif
