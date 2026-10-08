#include "chip8.h"
#include <SD.h>

uint8_t memory[4096];
uint8_t V[16];        
uint16_t I;           
uint16_t pc;          
uint8_t gfx[64 * 32]; 
bool drawFlag;
bool keys[16];

// Subroutine Stack
uint16_t stack[16];
uint16_t sp;

// Timers
uint8_t delay_timer;
uint8_t sound_timer;
unsigned long lastTimerTick;

unsigned char chip8_fontset[80] = { 
  0xF0, 0x90, 0x90, 0x90, 0xF0, 0x20, 0x60, 0x20, 0x20, 0x70, 
  0xF0, 0x10, 0xF0, 0x80, 0xF0, 0xF0, 0x10, 0xF0, 0x10, 0xF0, 
  0x90, 0x90, 0xF0, 0x10, 0x10, 0xF0, 0x80, 0xF0, 0x10, 0xF0, 
  0xF0, 0x80, 0xF0, 0x90, 0xF0, 0xF0, 0x10, 0x20, 0x40, 0x40, 
  0xF0, 0x90, 0xF0, 0x90, 0xF0, 0xF0, 0x90, 0xF0, 0x10, 0xF0, 
  0xF0, 0x90, 0xF0, 0x90, 0x90, 0xE0, 0x90, 0xE0, 0x90, 0xE0, 
  0xF0, 0x80, 0x80, 0x80, 0xF0, 0xE0, 0x90, 0x90, 0x90, 0xE0, 
  0xF0, 0x80, 0xF0, 0x80, 0xF0, 0xF0, 0x80, 0xF0, 0x80, 0x80  
};

void initChip8() {
    pc = 0x200; 
    I = 0;
    sp = 0;
    drawFlag = true;
    delay_timer = 0;
    sound_timer = 0;
    lastTimerTick = millis();

    memset(gfx, 0, sizeof(gfx));
    memset(memory, 0, sizeof(memory));
    memset(V, 0, sizeof(V));
    memset(stack, 0, sizeof(stack));
    memset(keys, 0, sizeof(keys));

    for (int i = 0; i < 80; ++i) { memory[i] = chip8_fontset[i]; }
}

bool loadROM(String filename) {
    initChip8();
    File rom = SD.open(filename, FILE_READ);
    if (!rom) return false;

    // FIX: Read the entire file into memory at once. 
    // This drops the 30-second load time down to 0.1 seconds!
    size_t fileSize = rom.size();
    if (fileSize > (4096 - 512)) fileSize = 4096 - 512; 
    
    rom.read(&memory[512], fileSize);
    rom.close();
    
    return true;
}

void emulateCycle() {
    uint16_t opcode = memory[pc] << 8 | memory[pc + 1];
    uint8_t X = (opcode & 0x0F00) >> 8;
    uint8_t Y = (opcode & 0x00F0) >> 4;

    switch (opcode & 0xF000) {
        case 0x0000:
            if (opcode == 0x00E0) { memset(gfx, 0, sizeof(gfx)); drawFlag = true; pc += 2; }
            else if (opcode == 0x00EE) { --sp; pc = stack[sp]; pc += 2; }
            else { pc += 2; }
            break;
        case 0x1000: pc = opcode & 0x0FFF; break;
        case 0x2000: stack[sp] = pc; ++sp; pc = opcode & 0x0FFF; break;
        case 0x3000: if (V[X] == (opcode & 0x00FF)) pc += 4; else pc += 2; break;
        case 0x4000: if (V[X] != (opcode & 0x00FF)) pc += 4; else pc += 2; break;
        case 0x5000: if (V[X] == V[Y]) pc += 4; else pc += 2; break;
        case 0x6000: V[X] = opcode & 0x00FF; pc += 2; break;
        case 0x7000: V[X] += opcode & 0x00FF; pc += 2; break;
        case 0x8000:
            switch(opcode & 0x000F) {
                case 0x0000: V[X] = V[Y]; pc += 2; break;
                case 0x0001: V[X] |= V[Y]; pc += 2; break;
                case 0x0002: V[X] &= V[Y]; pc += 2; break;
                case 0x0003: V[X] ^= V[Y]; pc += 2; break;
                case 0x0004: V[0xF] = (V[Y] > (0xFF - V[X])) ? 1 : 0; V[X] += V[Y]; pc += 2; break;
                case 0x0005: V[0xF] = (V[X] > V[Y]) ? 1 : 0; V[X] -= V[Y]; pc += 2; break;
                case 0x0006: V[0xF] = V[X] & 0x1; V[X] >>= 1; pc += 2; break;
                case 0x0007: V[0xF] = (V[Y] > V[X]) ? 1 : 0; V[X] = V[Y] - V[X]; pc += 2; break;
                case 0x000E: V[0xF] = V[X] >> 7; V[X] <<= 1; pc += 2; break;
            } break;
        case 0x9000: if (V[X] != V[Y]) pc += 4; else pc += 2; break;
        case 0xA000: I = opcode & 0x0FFF; pc += 2; break;
        case 0xB000: pc = (opcode & 0x0FFF) + V[0]; break;
        case 0xC000: V[X] = (rand() % 0xFF) & (opcode & 0x00FF); pc += 2; break;
        case 0xD000: {
            uint16_t x = V[X];
            uint16_t y = V[Y];
            uint16_t height = opcode & 0x000F;
            V[0xF] = 0;
            for (int yline = 0; yline < height; yline++) {
                uint16_t pixel = memory[I + yline];
                for (int xline = 0; xline < 8; xline++) {
                    if ((pixel & (0x80 >> xline)) != 0) {
                        int index = ((x + xline) % 64) + (((y + yline) % 32) * 64);
                        if (gfx[index] == 1) V[0xF] = 1;                                 
                        gfx[index] ^= 1;
                    }
                }
            }
            drawFlag = true; pc += 2;
        } break;
        case 0xE000:
            switch(opcode & 0x00FF) {
                case 0x009E: if (keys[V[X]] != 0) pc += 4; else pc += 2; break;
                case 0x00A1: if (keys[V[X]] == 0) pc += 4; else pc += 2; break;
            } break;
        case 0xF000:
            switch(opcode & 0x00FF) {
                case 0x0007: V[X] = delay_timer; pc += 2; break;
                case 0x000A: {
                    bool keyPress = false;
                    for(int i = 0; i < 16; ++i) { if(keys[i]) { V[X] = i; keyPress = true; } }
                    if(!keyPress) return; pc += 2; 
                } break;
                case 0x0015: delay_timer = V[X]; pc += 2; break;
                case 0x0018: sound_timer = V[X]; pc += 2; break;
                case 0x001E: I += V[X]; pc += 2; break;
                case 0x0029: I = V[X] * 0x5; pc += 2; break;
                case 0x0033: memory[I] = V[X] / 100; memory[I + 1] = (V[X] / 10) % 10; memory[I + 2] = (V[X] % 100) % 10; pc += 2; break;
                case 0x0055: for (int i = 0; i <= X; ++i) memory[I + i] = V[i]; I += X + 1; pc += 2; break;
                case 0x0065: for (int i = 0; i <= X; ++i) V[i] = memory[I + i]; I += X + 1; pc += 2; break;
            } break;
        default: pc += 2; 
    }

    // Process Timers (60Hz)
    if (millis() - lastTimerTick > 16) {
        if (delay_timer > 0) --delay_timer;
        if (sound_timer > 0) --sound_timer;
        lastTimerTick = millis();
    }
}