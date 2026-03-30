#ifndef GPIO_DIRECT_H
#define GPIO_DIRECT_H

#include <stdint.h>

/* Initialize GPIO via /dev/mem (BCM2836 base). Must run as root. */
int gpio_direct_init(void);

/* Pin mode */
void gpio_set_input(int pin);
void gpio_set_output(int pin);

/* Read/write */
int  gpio_read(int pin);
void gpio_write(int pin, int value);

/* Pull-up resistor */
void gpio_set_pullup(int pin);

/* Millisecond timer (replaces wiringPi millis()) */
unsigned long gpio_millis(void);

#endif
