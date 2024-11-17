#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LCD_RS  17
#define LCD_E   27
#define LCD_D4  25
#define LCD_D5  22
#define LCD_D6  16
#define LCD_D7  24

void lcdCommand(int command) {
    digitalWrite(LCD_RS, 0); // Command mode
    digitalWrite(LCD_D4, (command >> 4) & 0x01);
    digitalWrite(LCD_D5, (command >> 5) & 0x01);
    digitalWrite(LCD_D6, (command >> 6) & 0x01);
    digitalWrite(LCD_D7, (command >> 7) & 0x01);
    digitalWrite(LCD_E, 1);
    delayMicroseconds(1);
    digitalWrite(LCD_E, 0);
    delayMicroseconds(100);
    
    digitalWrite(LCD_D4, command & 0x01);
    digitalWrite(LCD_D5, (command >> 1) & 0x01);
    digitalWrite(LCD_D6, (command >> 2) & 0x01);
    digitalWrite(LCD_D7, (command >> 3) & 0x01);
    digitalWrite(LCD_E, 1);
    delayMicroseconds(1);
    digitalWrite(LCD_E, 0);
    delayMicroseconds(100);
}

void lcdInit() {
    wiringPiSetup();
    pinMode(LCD_RS, OUTPUT);
    pinMode(LCD_E, OUTPUT);
    pinMode(LCD_D4, OUTPUT);
    pinMode(LCD_D5, OUTPUT);
    pinMode(LCD_D6, OUTPUT);
    pinMode(LCD_D7, OUTPUT);
    
    delay(20); // Wait for LCD to power up
    lcdCommand(0x33); // Initialize
    lcdCommand(0x32); // Set to 4-bit mode
    lcdCommand(0x28); // 2 lines, 5x7 matrix
    lcdCommand(0x0C); // Display on, cursor off
    lcdCommand(0x06); // Increment cursor
    lcdCommand(0x01); // Clear display
    delay(2);
}

void lcdPrint(const char *str) {
    digitalWrite(LCD_RS, 1); // Data mode
    while (*str) {
        lcdCommand(*str++);
    }
}

void lcdClear() {
    lcdCommand(0x01); // Clear display
    delay(2);
}

void lcdPosition(int col, int row) {
    int command;
    if (row == 0) {
        command = 0x80 + col; // First line
    } else if (row == 1) {
        command = 0xC0 + col; // Second line
    } else {
        return; // Invalid row
    }
    lcdCommand(command);
}