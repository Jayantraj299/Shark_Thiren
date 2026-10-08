#ifndef CHIP8_H
#define CHIP8_H
#include <Arduino.h>

void initChip8();
bool loadROM(String filename);
void emulateCycle();

extern uint8_t gfx[64 * 32]; 
extern bool drawFlag;       
extern bool keys[16]; // NEW: 16-button virtual keypad

#endif