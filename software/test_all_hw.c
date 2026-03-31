/*
 * ScratchTJ v2 - Full hardware test (C)
 *
 * Tests all hardware simultaneously and displays on TFT:
 *   - ST7789 240x240 TFT via SPI (DC=GPIO24, RST=GPIO25)
 *   - MT6701 hall sensor via I2C (addr 0x06)
 *   - EC11 rotary encoder (A=GPIO23, B=GPIO22, BTN=GPIO27)
 *   - Arduino serial: fader + cap sensor + 4 buttons
 *
 * Build:  gcc -O2 -o test_all_hw test_all_hw.c -lm -lpthread
 * Run:    sudo ./test_all_hw
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/spi/spidev.h>
#include <linux/i2c-dev.h>
#include <termios.h>
#include <pthread.h>

/* ── Pin definitions ─────────────────────────────────────────────── */
#define TFT_DC    24
#define TFT_RST   25
#define ENC_A     23
#define ENC_B     22
#define ENC_BTN   27
#define ENC_KO    17

#define TFT_W     240
#define TFT_H     240

#define MT6701_ADDR   0x06
#define MT6701_ANGLE_H 0x03
#define MT6701_ANGLE_L 0x04

#define SERIAL_DEV    "/dev/serial0"
#define SERIAL_BAUD   B500000

#define SYNC_BYTE     0xAA
#define HANDSHAKE_MAGIC 0x53

/* ── GPIO via /dev/mem (BCM2836) ─────────────────────────────────── */
#define GPIO_BASE  0x3F200000
#define GPIO_LEN   0x100
#define GPFSEL0    0
#define GPSET0     7
#define GPCLR0     10
#define GPLEV0     13
#define GPPUD      37
#define GPPUDCLK0  38

static volatile uint32_t *gpio;

static void gpio_init(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); exit(1); }
    gpio = mmap(NULL, GPIO_LEN, PROT_READ | PROT_WRITE,
                MAP_SHARED, fd, GPIO_BASE);
    close(fd);
    if (gpio == MAP_FAILED) { perror("mmap gpio"); exit(1); }
}

static void gpio_mode_in(int pin)
{
    int reg = pin / 10, shift = (pin % 10) * 3;
    gpio[reg] &= ~(7 << shift);
}

static void gpio_mode_out(int pin)
{
    int reg = pin / 10, shift = (pin % 10) * 3;
    gpio[reg] &= ~(7 << shift);
    gpio[reg] |= (1 << shift);
}

static void gpio_set(int pin)   { gpio[GPSET0] = 1u << pin; }
static void gpio_clr(int pin)   { gpio[GPCLR0] = 1u << pin; }
static int  gpio_get(int pin)   { return (gpio[GPLEV0] >> pin) & 1; }

static void gpio_pullup(int pin)
{
    gpio[GPPUD] = 2;  /* pull-up */
    usleep(10);
    gpio[GPPUDCLK0] = 1u << pin;
    usleep(10);
    gpio[GPPUD] = 0;
    gpio[GPPUDCLK0] = 0;
}

/* ── SPI / TFT ───────────────────────────────────────────────────── */
static int spi_fd;

static void spi_init(void)
{
    spi_fd = open("/dev/spidev0.0", O_RDWR);
    if (spi_fd < 0) { perror("open spidev"); exit(1); }
    uint8_t mode = SPI_MODE_0;
    uint32_t speed = 40000000;
    uint8_t bits = 8;
    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
}

static void tft_cmd(uint8_t c)
{
    gpio_clr(TFT_DC);
    write(spi_fd, &c, 1);
}

static void tft_data(const uint8_t *d, int len)
{
    gpio_set(TFT_DC);
    while (len > 0) {
        int n = len > 4096 ? 4096 : len;
        write(spi_fd, d, n);
        d += n; len -= n;
    }
}

static void tft_cmd_d(uint8_t c, const uint8_t *d, int len)
{
    tft_cmd(c);
    if (len > 0) tft_data(d, len);
}

static void tft_reset(void)
{
    gpio_set(TFT_RST); usleep(100000);
    gpio_clr(TFT_RST); usleep(100000);
    gpio_set(TFT_RST); usleep(200000);
}

static void tft_init(void)
{
    gpio_mode_out(TFT_DC);
    gpio_mode_out(TFT_RST);
    spi_init();
    tft_reset();

    tft_cmd(0x01); usleep(200000);  /* SW reset */
    tft_cmd(0x11); usleep(200000);  /* sleep out */

    uint8_t porch[] = {0x0C,0x0C,0x00,0x33,0x33};
    tft_cmd_d(0xB2, porch, 5);
    tft_cmd_d(0xB7, (uint8_t[]){0x35}, 1);
    tft_cmd_d(0xBB, (uint8_t[]){0x19}, 1);
    tft_cmd_d(0xC0, (uint8_t[]){0x2C}, 1);
    tft_cmd_d(0xC2, (uint8_t[]){0x01}, 1);
    tft_cmd_d(0xC3, (uint8_t[]){0x12}, 1);
    tft_cmd_d(0xC4, (uint8_t[]){0x20}, 1);
    tft_cmd_d(0xC6, (uint8_t[]){0x0F}, 1);
    tft_cmd_d(0xD0, (uint8_t[]){0xA4,0xA1}, 2);

    uint8_t pgam[] = {0xD0,0x04,0x0D,0x11,0x13,0x2B,0x3F,0x54,
                      0x4C,0x18,0x0D,0x0B,0x1F,0x23};
    tft_cmd_d(0xE0, pgam, 14);
    uint8_t ngam[] = {0xD0,0x04,0x0C,0x11,0x13,0x2C,0x3F,0x44,
                      0x51,0x2F,0x1F,0x1F,0x20,0x23};
    tft_cmd_d(0xE1, ngam, 14);

    tft_cmd_d(0x3A, (uint8_t[]){0x05}, 1);  /* 16-bit color */
    tft_cmd_d(0x36, (uint8_t[]){0x60}, 1);  /* MADCTL: 90° CW */
    tft_cmd(0x21);                            /* inversion on */
    tft_cmd(0x13);                            /* normal mode */
    tft_cmd(0x29);                            /* display on */
    usleep(100000);
}

static void tft_update(const uint16_t *fb)
{
    uint8_t ca[] = {0,0, 0,TFT_W-1};
    tft_cmd_d(0x2A, ca, 4);
    uint8_t ra[] = {0,0, 0,TFT_H-1};
    tft_cmd_d(0x2B, ra, 4);
    tft_cmd(0x2C);
    tft_data((const uint8_t *)fb, TFT_W * TFT_H * 2);
}

/* ── I2C / MT6701 ────────────────────────────────────────────────── */
static int i2c_fd = -1;

static int mt6701_init(void)
{
    i2c_fd = open("/dev/i2c-1", O_RDWR);
    if (i2c_fd < 0) { perror("open i2c"); return -1; }
    if (ioctl(i2c_fd, I2C_SLAVE, MT6701_ADDR) < 0) {
        perror("i2c set addr"); close(i2c_fd); i2c_fd = -1; return -1;
    }
    return 0;
}

static float mt6701_read(void)
{
    if (i2c_fd < 0) return -1.0f;
    uint8_t reg = MT6701_ANGLE_H;
    if (write(i2c_fd, &reg, 1) != 1) return -1.0f;
    uint8_t buf[2];
    if (read(i2c_fd, buf, 2) != 2) return -1.0f;
    /* 14-bit angle: reg03[7:0] = bits 13..6, reg04[7:2] = bits 5..0 */
    uint16_t raw = ((uint16_t)buf[0] << 6) | (buf[1] >> 2);
    return raw * 360.0f / 16384.0f;
}

/* ── Serial / Arduino ────────────────────────────────────────────── */
static int ser_fd = -1;

struct arduino_pkt {
    int fader;       /* 0-1023 */
    int cap;         /* 0-65535 */
    uint8_t buttons; /* bit0=A0 .. bit3=A3, 1=pressed */
    int valid;
};

static int serial_init(void)
{
    ser_fd = open(SERIAL_DEV, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (ser_fd < 0) { perror("open serial"); return -1; }

    struct termios t;
    tcgetattr(ser_fd, &t);
    cfmakeraw(&t);
    cfsetispeed(&t, SERIAL_BAUD);
    cfsetospeed(&t, SERIAL_BAUD);
    t.c_cflag |= CLOCAL | CREAD;
    t.c_cflag &= ~CRTSCTS;
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(ser_fd, TCSANOW, &t);
    tcflush(ser_fd, TCIOFLUSH);

    /* Send handshake */
    uint8_t hs = HANDSHAKE_MAGIC;
    write(ser_fd, &hs, 1);
    usleep(100000);
    return 0;
}

static int ser_bytes_total = 0;
static int ser_pkts_ok = 0;
static int ser_pkts_bad = 0;

static int serial_read(struct arduino_pkt *pkt)
{
    static uint8_t ring[256];
    static int rpos = 0, wpos = 0;

    /* Fill ring buffer */
    uint8_t tmp[64];
    int n = read(ser_fd, tmp, sizeof(tmp));
    if (n > 0) {
        ser_bytes_total += n;
        for (int i = 0; i < n; i++) {
            ring[wpos++ & 0xFF] = tmp[i];
        }
    }

    /* Scan for valid packet (7 bytes: SYNC f f c c btn chk) */
    int avail = (wpos - rpos) & 0xFF;
    while (avail >= 7) {
        if (ring[rpos & 0xFF] != SYNC_BYTE) {
            rpos++; avail--; continue;
        }
        uint8_t p[7];
        for (int i = 0; i < 7; i++)
            p[i] = ring[(rpos + i) & 0xFF];

        uint8_t chk = p[1] ^ p[2] ^ p[3] ^ p[4] ^ p[5];
        if (chk == p[6]) {
            pkt->fader   = (p[1] << 8) | p[2];
            pkt->cap     = (p[3] << 8) | p[4];
            pkt->buttons = p[5];
            pkt->valid   = 1;
            rpos += 7;
            ser_pkts_ok++;
            return 1;
        }
        ser_pkts_bad++;
        rpos++; avail--;
    }
    return 0;
}

static volatile int running = 1;

/* ── EC11 encoder (dedicated polling thread) ─────────────────────── */
static volatile int enc_pos = 0;
static volatile int enc_btn = 0;
static volatile int enc_ko = 0;
static pthread_t enc_thread;

static void *ec11_thread_fn(void *arg)
{
    (void)arg;
    int last_a = gpio_get(ENC_A);

    while (running) {
        int a = gpio_get(ENC_A);

        /* Count on rising edge of A only = 1 step per detent */
        if (a && !last_a) {
            int b = gpio_get(ENC_B);
            enc_pos += b ? -1 : 1;
        }
        last_a = a;

        enc_btn = !gpio_get(ENC_BTN);  /* active low */
        enc_ko  = !gpio_get(ENC_KO);   /* active low */
        usleep(200);  /* ~5kHz polling */
    }
    return NULL;
}

static void ec11_init(void)
{
    gpio_mode_in(ENC_A);
    gpio_mode_in(ENC_B);
    gpio_mode_in(ENC_BTN);
    gpio_mode_in(ENC_KO);
    gpio_pullup(ENC_A);
    gpio_pullup(ENC_B);
    gpio_pullup(ENC_BTN);
    gpio_pullup(ENC_KO);
    pthread_create(&enc_thread, NULL, ec11_thread_fn, NULL);
}

/* ── Framebuffer & font ──────────────────────────────────────────── */
static uint16_t fb[TFT_W * TFT_H];

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return __builtin_bswap16(c);  /* big-endian for SPI */
}

static void fb_clear(uint16_t c)
{
    for (int i = 0; i < TFT_W * TFT_H; i++) fb[i] = c;
}

static void fb_hline(int x0, int x1, int y, uint16_t c)
{
    if (y < 0 || y >= TFT_H) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= TFT_W) x1 = TFT_W - 1;
    for (int x = x0; x <= x1; x++) fb[y * TFT_W + x] = c;
}

static void fb_line(int x0, int y0, int x1, int y1, uint16_t c)
{
    int dx = abs(x1-x0), sx = x0<x1?1:-1;
    int dy = -abs(y1-y0), sy = y0<y1?1:-1;
    int err = dx+dy;
    for (;;) {
        if (x0>=0 && x0<TFT_W && y0>=0 && y0<TFT_H)
            fb[y0*TFT_W+x0] = c;
        if (x0==x1 && y0==y1) break;
        int e2 = 2*err;
        if (e2>=dy) { err+=dy; x0+=sx; }
        if (e2<=dx) { err+=dx; y0+=sy; }
    }
}

static void fb_circle(int cx, int cy, int r, uint16_t c)
{
    int x=r, y=0, err=1-r;
    while (x>=y) {
        int pts[][2] = {{cx+x,cy+y},{cx-x,cy+y},{cx+x,cy-y},{cx-x,cy-y},
                        {cx+y,cy+x},{cx-y,cy+x},{cx+y,cy-x},{cx-y,cy-x}};
        for (int i=0;i<8;i++)
            if (pts[i][0]>=0&&pts[i][0]<TFT_W&&pts[i][1]>=0&&pts[i][1]<TFT_H)
                fb[pts[i][1]*TFT_W+pts[i][0]]=c;
        y++;
        if (err<0) err+=2*y+1;
        else { x--; err+=2*(y-x)+1; }
    }
}

static void fb_fill_rect(int x, int y, int w, int h, uint16_t c)
{
    for (int j = y; j < y+h && j < TFT_H; j++)
        for (int i = x; i < x+w && i < TFT_W; i++)
            if (i>=0 && j>=0) fb[j*TFT_W+i] = c;
}

/* 8x8 font - printable ASCII 32-90 + some symbols */
static const uint8_t font8x8[128][8] = {
    [' '] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    ['!'] = {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00},
    ['+'] = {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00},
    ['-'] = {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00},
    ['.'] = {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
    ['/'] = {0x02,0x06,0x0C,0x18,0x30,0x60,0x40,0x00},
    ['0'] = {0x3C,0x66,0x6E,0x7E,0x76,0x66,0x3C,0x00},
    ['1'] = {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00},
    ['2'] = {0x3C,0x66,0x06,0x1C,0x30,0x66,0x7E,0x00},
    ['3'] = {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00},
    ['4'] = {0x0E,0x1E,0x36,0x66,0x7F,0x06,0x06,0x00},
    ['5'] = {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00},
    ['6'] = {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00},
    ['7'] = {0x7E,0x66,0x06,0x0C,0x18,0x18,0x18,0x00},
    ['8'] = {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00},
    ['9'] = {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00},
    [':'] = {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00},
    ['='] = {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00},
    ['A'] = {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00},
    ['B'] = {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00},
    ['C'] = {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00},
    ['D'] = {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00},
    ['E'] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00},
    ['F'] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00},
    ['G'] = {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3E,0x00},
    ['H'] = {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00},
    ['I'] = {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00},
    ['J'] = {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00},
    ['K'] = {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00},
    ['L'] = {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00},
    ['M'] = {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00},
    ['N'] = {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00},
    ['O'] = {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    ['P'] = {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00},
    ['Q'] = {0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0x00},
    ['R'] = {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00},
    ['S'] = {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00},
    ['T'] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    ['U'] = {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    ['V'] = {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00},
    ['W'] = {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    ['X'] = {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00},
    ['Y'] = {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00},
    ['Z'] = {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00},
    ['a'] = {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00},
    ['b'] = {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00},
    ['c'] = {0x00,0x00,0x3C,0x66,0x60,0x66,0x3C,0x00},
    ['d'] = {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00},
    ['e'] = {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00},
    ['f'] = {0x1C,0x36,0x30,0x7C,0x30,0x30,0x30,0x00},
    ['g'] = {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x3C},
    ['h'] = {0x60,0x60,0x6C,0x76,0x66,0x66,0x66,0x00},
    ['i'] = {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00},
    ['k'] = {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00},
    ['l'] = {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00},
    ['m'] = {0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00},
    ['n'] = {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00},
    ['o'] = {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00},
    ['p'] = {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60},
    ['r'] = {0x00,0x00,0x6C,0x76,0x60,0x60,0x60,0x00},
    ['s'] = {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00},
    ['t'] = {0x30,0x30,0x7C,0x30,0x30,0x36,0x1C,0x00},
    ['u'] = {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00},
    ['v'] = {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00},
    ['w'] = {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00},
    ['x'] = {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00},
    ['y'] = {0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x3C},
    ['z'] = {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00},
    ['|'] = {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
};

static void fb_char(int x, int y, char ch, uint16_t col, int scale)
{
    uint8_t c = (uint8_t)ch;
    if (c >= 128) return;
    const uint8_t *glyph = font8x8[c];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col_i = 0; col_i < 8; col_i++) {
            if (bits & (0x80 >> col_i)) {
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++) {
                        int px = x + col_i*scale + sx;
                        int py = y + row*scale + sy;
                        if (px>=0 && px<TFT_W && py>=0 && py<TFT_H)
                            fb[py*TFT_W+px] = col;
                    }
            }
        }
    }
}

static void fb_text(int x, int y, const char *s, uint16_t col, int scale)
{
    while (*s) {
        fb_char(x, y, *s, col, scale);
        x += 8 * scale;
        s++;
    }
}

/* ── Main ────────────────────────────────────────────────────────── */
static void sig_handler(int sig) { (void)sig; running = 0; }

int main(void)
{
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    printf("=== ScratchTJ v2 Full Hardware Test ===\n\n");

    /* Init GPIO */
    gpio_init();

    /* Init TFT */
    printf("TFT init...");
    tft_init();
    printf(" OK\n");

    /* Quick color test */
    uint16_t colors[] = { rgb565(255,0,0), rgb565(0,255,0), rgb565(0,0,255) };
    const char *cnames[] = { "RED", "GREEN", "BLUE" };
    for (int i = 0; i < 3; i++) {
        printf("  %s\n", cnames[i]);
        fb_clear(colors[i]);
        tft_update(fb);
        usleep(500000);
    }

    /* Init MT6701 */
    printf("MT6701 init...");
    if (mt6701_init() == 0) {
        float a = mt6701_read();
        if (a >= 0) printf(" OK (angle=%.1f)\n", a);
        else        printf(" NO RESPONSE (check wiring)\n");
    } else {
        printf(" FAILED\n");
    }

    /* Init EC11 */
    printf("EC11 init...");
    ec11_init();
    printf(" OK\n");

    /* Init serial */
    printf("Arduino serial init...");
    if (serial_init() == 0) printf(" OK\n");
    else                     printf(" FAILED\n");

    printf("\n=== LIVE TEST === (Ctrl+C to exit)\n\n");

    /* Colors */
    uint16_t C_BG     = rgb565(0, 0, 0);
    uint16_t C_TITLE  = rgb565(0, 200, 255);
    uint16_t C_LABEL  = rgb565(180, 180, 180);
    uint16_t C_VAL    = rgb565(0, 255, 100);
    uint16_t C_WARN   = rgb565(255, 50, 50);
    uint16_t C_BTN_ON = rgb565(0, 255, 0);
    uint16_t C_BTN_OFF= rgb565(60, 60, 60);
    uint16_t C_LINE   = rgb565(0, 80, 100);
    uint16_t C_ARC    = rgb565(40, 40, 40);
    uint16_t C_YELLOW = rgb565(255, 255, 0);

    struct arduino_pkt apkt = {0};
    int frame = 0;

    while (running) {
        /* Read inputs */
        float mt_angle = (i2c_fd >= 0) ? mt6701_read() : -1.0f;
        int btn = enc_btn;
        int ko = enc_ko;

        /* Drain serial */
        struct arduino_pkt tmp;
        while (serial_read(&tmp))
            apkt = tmp;

        /* Draw frame */
        fb_clear(C_BG);

        /* Title */
        fb_text(4, 4, "SCRATCHTJ HW TEST", C_TITLE, 2);
        fb_hline(4, 235, 24, C_LINE);

        /* MT6701 section */
        int y = 30;
        fb_text(4, y, "MT6701 HALL:", C_LABEL, 1);
        char buf[32];
        if (mt_angle >= 0) {
            snprintf(buf, sizeof(buf), "%.1f", mt_angle);
            fb_text(4, y+12, buf, C_VAL, 2);
            /* Angle indicator */
            int cx=200, cy=y+20, r=18;
            fb_circle(cx, cy, r, C_ARC);
            float rad = (mt_angle - 90.0f) * M_PI / 180.0f;
            int ex = cx + (int)(r * cosf(rad));
            int ey = cy + (int)(r * sinf(rad));
            fb_line(cx, cy, ex, ey, C_VAL);
        } else {
            fb_text(4, y+12, "NO SIGNAL", C_WARN, 2);
        }

        /* Separator */
        y = 72;
        fb_hline(4, 235, y, C_LINE);

        /* Arduino section */
        y += 4;
        fb_text(4, y, "ARDUINO SERIAL:", C_LABEL, 1);
        y += 12;
        if (apkt.valid) {
            snprintf(buf, sizeof(buf), "FADER: %4d", apkt.fader);
            fb_text(4, y, buf, C_VAL, 1); y += 12;
            snprintf(buf, sizeof(buf), "CAP:   %5d", apkt.cap);
            fb_text(4, y, buf, C_VAL, 1); y += 12;

            fb_text(4, y, "BTN:", C_LABEL, 1);
            for (int i = 0; i < 4; i++) {
                int pressed = (apkt.buttons >> i) & 1;
                snprintf(buf, sizeof(buf), "%d", i+1);
                uint16_t bc = pressed ? C_BTN_ON : C_BTN_OFF;
                fb_fill_rect(44 + i*28, y-1, 22, 10, bc);
                fb_text(48 + i*28, y, buf, C_BG, 1);
            }
            y += 14;

            /* Fader bar */
            fb_text(4, y, "FADER:", C_LABEL, 1);
            fb_fill_rect(56, y, 170, 8, C_ARC);
            int fw = apkt.fader * 170 / 1023;
            fb_fill_rect(56, y, fw, 8, C_VAL);
        } else {
            fb_text(4, y, "WAITING...", C_WARN, 1);
        }

        /* Separator */
        y += 16;
        fb_hline(4, 235, y, C_LINE);

        /* EC11 section */
        y += 4;
        fb_text(4, y, "EC11 ENCODER:", C_LABEL, 1);
        y += 12;
        snprintf(buf, sizeof(buf), "POS: %+d", enc_pos);
        fb_text(4, y, buf, C_YELLOW, 2);

        fb_text(140, y, "PSK", C_LABEL, 1);
        fb_fill_rect(168, y-1, 24, 10, btn ? C_BTN_ON : C_BTN_OFF);
        fb_text(140, y+14, "KO", C_LABEL, 1);
        fb_fill_rect(168, y+13, 24, 10, ko ? C_BTN_ON : C_BTN_OFF);

        /* Footer */
        y = TFT_H - 12;
        snprintf(buf, sizeof(buf), "frame %d", frame);
        fb_text(4, y, buf, C_ARC, 1);

        /* Update display */
        tft_update(fb);
        frame++;

        /* Console output every 10 frames */
        if (frame % 10 == 1) {
            printf("\r  MT:%.1f EC11:%+d PSK:%s KO:%s ARD:f=%d c=%d b=%02x [rx:%d ok:%d bad:%d]  f%d  ",
                   mt_angle, enc_pos, btn?"Y":"N", ko?"Y":"N",
                   apkt.fader, apkt.cap, apkt.buttons,
                   ser_bytes_total, ser_pkts_ok, ser_pkts_bad, frame);
            fflush(stdout);
        }

        usleep(20000);  /* ~50 fps target */
    }

    printf("\n\nShutting down...\n");
    pthread_join(enc_thread, NULL);
    fb_clear(C_BG);
    tft_update(fb);

    if (ser_fd >= 0) close(ser_fd);
    if (i2c_fd >= 0) close(i2c_fd);
    close(spi_fd);
    printf("Done.\n");
    return 0;
}
