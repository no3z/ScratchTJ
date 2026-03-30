#include "gpio_direct.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>

/* BCM2836 GPIO registers (Pi 2/3) */
#define GPIO_BASE   0x3F200000
#define GPIO_LEN    0x100
#define GPFSEL0     0
#define GPSET0      7
#define GPCLR0      10
#define GPLEV0      13
#define GPPUD       37
#define GPPUDCLK0   38

static volatile uint32_t *gpio_reg;
static struct timespec start_time;

int gpio_direct_init(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("gpio_direct: open /dev/mem");
        return -1;
    }
    gpio_reg = mmap(NULL, GPIO_LEN, PROT_READ | PROT_WRITE,
                    MAP_SHARED, fd, GPIO_BASE);
    close(fd);
    if (gpio_reg == MAP_FAILED) {
        perror("gpio_direct: mmap");
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    return 0;
}

void gpio_set_input(int pin)
{
    int reg = pin / 10, shift = (pin % 10) * 3;
    gpio_reg[reg] &= ~(7 << shift);
}

void gpio_set_output(int pin)
{
    int reg = pin / 10, shift = (pin % 10) * 3;
    gpio_reg[reg] &= ~(7 << shift);
    gpio_reg[reg] |= (1 << shift);
}

int gpio_read(int pin)
{
    return (gpio_reg[GPLEV0] >> pin) & 1;
}

void gpio_write(int pin, int value)
{
    if (value)
        gpio_reg[GPSET0] = 1u << pin;
    else
        gpio_reg[GPCLR0] = 1u << pin;
}

void gpio_set_pullup(int pin)
{
    gpio_reg[GPPUD] = 2;  /* pull-up */
    usleep(10);
    gpio_reg[GPPUDCLK0] = 1u << pin;
    usleep(10);
    gpio_reg[GPPUD] = 0;
    gpio_reg[GPPUDCLK0] = 0;
}

unsigned long gpio_millis(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (unsigned long)((now.tv_sec - start_time.tv_sec) * 1000 +
                           (now.tv_nsec - start_time.tv_nsec) / 1000000);
}
