# ScratchTJ hardware test programs
#
# C tests build here but link against sources in ../software/
# Python tests run directly: sudo python3 test_xxx.py

CC = gcc
CFLAGS = -O2 -Wall
SW = ../software

# test_all_hw: self-contained, no project deps
test_all_hw: test_all_hw.c
	$(CC) $(CFLAGS) -o $@ $< -lm -lpthread

# test_tft_ui: depends on display/gpio modules from software/
test_tft_ui: test_tft_ui.o gpio_direct.o st7789.o oled_display.o
	$(CC) -o $@ $^ -lm

test_tft_ui.o: test_tft_ui.c
	$(CC) $(CFLAGS) -I$(SW) -c -o $@ $<

gpio_direct.o: $(SW)/gpio_direct.c $(SW)/gpio_direct.h
	$(CC) $(CFLAGS) -I$(SW) -c -o $@ $<

st7789.o: $(SW)/st7789.c $(SW)/st7789.h
	$(CC) $(CFLAGS) -I$(SW) -c -o $@ $<

oled_display.o: $(SW)/oled_display.c $(SW)/oled_display.h
	$(CC) $(CFLAGS) -I$(SW) -c -o $@ $<

all: test_all_hw test_tft_ui

clean:
	rm -f *.o test_all_hw test_tft_ui

.PHONY: all clean
