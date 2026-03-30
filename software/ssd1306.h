#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT 64
#define SSD1306_I2C_ADDR 0x3C

/* Font sizes */
#define FONT_SMALL  0   /* 6x8:  21 chars x 8 lines */
#define FONT_LARGE  1   /* 12x16: pixel-doubled from 6x8 */

int  ssd1306_init(const char *i2c_device);
void ssd1306_close(void);
void ssd1306_clear(void);
void ssd1306_pixel(int x, int y, int on);
void ssd1306_hline(int x, int y, int w);
void ssd1306_vline(int x, int y, int h);
void ssd1306_fill_rect(int x, int y, int w, int h, int on);
void ssd1306_draw_char(int x, int y, char c, int font_size);
void ssd1306_draw_string(int x, int y, const char *str, int font_size);
int  ssd1306_string_width(const char *str, int font_size);
void ssd1306_flush(void);

#endif
