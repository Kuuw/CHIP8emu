#include "chip8.h"
#include "SDL3/SDL.h"
#include <iostream>

chip8 CHIP8;


int main(int argc, char* argv[]) {
    SDL_Log("%s", "Starting CHIP-8 Emulator...");
    CHIP8.initialize();

    for (;;) {
        CHIP8.emulate_cycle(); // Emulate one cycle
        if (CHIP8.drawFlag) {
            // TODO: Implement the rendering logic here
            CHIP8.drawFlag = false; // Reset the draw flag
        }
        SDL_Delay(16); // Delay to control the speed of emulation (approx. 60Hz)
    }

    return 0;
}

void chip8::initialize() {
    // Initialize the emulator state
    opcode = 0;
    I = 0;
    pc = 0x200; // The Program starts at 0x200
    sp = 0;
    // Reset timers
    delay_timer = 0;
    sound_timer = 0;

    // Clear memory
    for (unsigned char & i : memory) { i = 0; }

    // Clear registers
    for (unsigned char & i : V) { i = 0; }

    // Clear graphics buffer
    for (unsigned char & i : gfx) { i = 0; }

    // Clear keypad
    for (unsigned char & i : key) { i = 0; }

    // Clear stack
    for (unsigned char & i : stack) { i = 0; }

    // Load the font set into memory
    // The font set is stored at the beginning of memory
    for (int i=0; i < 80; i++) {
        memory[i] = chip8_font[i];
    }
}

void chip8::emulate_cycle() {
    opcode = memory[pc] << 8 | memory[pc + 1]; // Fetch opcode
    switch (opcode & 0xF000) {
        case 0x0000:
            switch (opcode & 0x00FF) {
                case 0x00E0: // Clear the display
                    for (unsigned char & i : gfx) { i = 0; }
                    break;
                case 0x00EE: // Return from subroutine
                    sp--;
                    pc = stack[sp];
                    break;
                default:
                    std::cerr << "Unknown opcode: " << std::hex << opcode << std::endl;
            }
            break;
        case 0x1000: // 0x1NNN - Jump to address NNN
            pc = opcode & 0x0FFF;
            break;
        case 0x2000: // 0x2NNN - Call subroutine at NNN
            stack[sp] = pc;
            ++sp;
            if (sp > 15) { // Check for stack overflow
                std::cerr << "Stack overflow!" << std::endl;
                return;
            }
            pc = opcode & 0x0FFF;
            break;
        case 0x3000: // 0x3XNN - Skip next instruction if VX == NN
            if (V[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF)) {
                pc += 2;
            }
            break;
        case 0x4000: // 0x4XNN - Skip next instruction if VX != NN
            if (V[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF)) {
                pc += 2;
            }
            break;
        case 0x5000: // 0x5XY0 - Skip next instruction if VX == VY
            if (V[(opcode & 0x0F00) >> 8] == V[(opcode & 0x00F0) >> 4]) {
                pc += 2;
            }
            break;
        case 0x6000: // 0x6XNN - Set VX = NN.
            V[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
            break;
        case 0x7000: // 0x7XNN - Set VX = VX + NN.
            V[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
        case 0x8000:
            switch (opcode & 0x000F) { // 0x8XY-
                case 0x0000: // Sets VX to the value of VY.
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4];
                    break;
                case 0x0001: // Sets VX to VX or VY (bitwise).
                    V[(opcode & 0x0F00) >> 8] |= V[(opcode & 0x00F0) >> 4];
                    break;
                case 0x0002: // Sets VX to VX and VY (bitwise).
                    V[(opcode & 0x0F00) >> 8] &= V[(opcode & 0x00F0) >> 4];
                    break;
                case 0x0003: // Sets VX to VX xor VY (bitwise).
                    V[(opcode & 0x0F00) >> 8] ^= V[(opcode & 0x00F0) >> 4];
                    break;
                case 0x0004: // Adds VY to VX, VF is set to 1 when there is a overflow, and 0 when there is not.
                    if (V[(opcode & 0x00F0) >> 4] > (0xFF - V[(opcode & 0x0F00) >> 8])) {
                        V[0xF] = 1; // Set carry flag
                    } else {
                        V[0xF] = 0; // Clear carry flag
                    }
                    V[(opcode & 0x0F00) >> 8] += V[(opcode & 0x00F0) >> 4];
                    break;
                case 0x0005: // VY is subtracted from VX. VF is set to 0 when there's an underflow, and 1 when there is not.
                    if (V[(opcode & 0x00F0) >> 4] <= V[opcode & 0x0F00]) {
                        V[0xF] = 1;
                    } else {
                        V[0xF] = 0;
                    }
                    V[(opcode & 0x0F00) >> 8] -= V[(opcode & 0x00F0) >> 4];
                case 0x0006: // Shifts VX to the right by 1, then stores the least significant bit of VX prior to the shift into VF
                    V[0xF] = V[(opcode & 0x0F00) >> 8] & 0x01;
                    V[(opcode & 0x0F00) >> 8] >>= 1;
                    break;
                case 0x0007: // Sets VX to VY - VX. VF is set to 0 when there's an underflow, and 1 when there is not.
                    if (V[(opcode & 0x0F00) >> 8] <= V[(opcode & 0x00F0) >> 4]) {
                        V[0xF] = 1;
                    } else {
                        V[0xF] = 0;
                    }
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4] - V[(opcode & 0x0F00) >> 8];
                    break;
                case 0x000E: // Shifts VX to the left by 1, then sets VF to 1 if the most significant bit of VX prior to that sift was set, or to 0 if it was unset.
                    V[0xF] = (V[(opcode & 0x0F00) >> 8] & 0x80) >> 7;
                    V[(opcode & 0x0F00) >> 8] <<= 1;
                    break;
                default:
                    std::cerr << "Unknown opcode: " << std::hex << opcode << std::endl;
            }
        case 0x9000: // 0x9XY0 - Skips next instruction if VX does not equal VY.
            if (V[(opcode & 0x0F00) >> 8] != V[(opcode & 0x00F0) >> 4]) {
                pc += 2;
            }
            break;
        case 0xA000: // 0xANNN - Sets I to the address NNN.
            I = opcode & 0x0FFF;
            break;
        case 0xB000: // 0xBNNN - Jumps to the address NNN plus V0.
            pc = (opcode & 0x0FFF) + V[0];
            break;
        case 0xC000: // 0xCXNN - Sets VX to the result of a bitwise and operation on a random number. (VX = rand() & NN)
            V[(opcode & 0x0F00) >> 8] = (rand() % 256) & (opcode & 0x00FF);
            break;
        case 0xD000: // 0xDXYN - Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels and height of N pixels. Each row of 8 pixels is read as bit-coded starting from memory location I; I value does not change after the execution of this instruction. As described above, VF is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn, and to 0 if that does not happen.
            for (int y = 0; y < (opcode & 0x000F); y++) {
                unsigned char sprite_row = memory[I + y];
                for (int x = 0; x < 8; x++) {
                    if ((sprite_row & (0x80 >> x)) != 0) { // Check if the pixel is set
                        int pixel_x = V[(opcode & 0x0F00) >> 8] + x;
                        int pixel_y = V[(opcode & 0x00F0) >> 4] + y;
                        if (pixel_x < 64 && pixel_y < 32) { // Check bounds
                            int index = pixel_y * 64 + pixel_x;
                            if (gfx[index] == 1) {
                                V[0xF] = 1; // Set collision flag
                            }
                            gfx[index] ^= 1; // Flip the pixel
                        }
                    }
                }
            }
            drawFlag = true; // Set the draw flag to indicate that the screen should be redrawn
            pc += 2;
            break;
        case 0xE000:
            switch (opcode & 0x00FF) {
                case 0x00A1: // 0xEXA1 - Skips the next instruction if the key stored in VX is not pressed. TODO: How can we check if a key is pressed?
                    if (key[V[(opcode & 0x0F00) >> 8]] == 0) {
                        pc += 2;
                    }
                    break;
                default:
                    std::cerr << "Unknown opcode: " << std::hex << opcode << std::endl;
            }
        case 0xF000:
            switch (opcode & 0x00FF) {
                case 0x000A: // 0xFX0A - A key press is awaited, and then stored in VX (blocking).
                    while (true) {
                        for (int i = 0; i < 16; i++) {
                            if (key[i] != 0) { // If a key is pressed
                                V[(opcode & 0x0F00) >> 8] = i; // Store the key in VX
                                return; // Exit the loop
                            }
                        }
                    }
                    break;
                case 0x001E: // 0xFX1E - Adds VX to I.
                    I += V[(opcode & 0x0F00) >> 8];
                    break;
                case 0x0007: // 0xFX07 - Sets VX to the value of the delay timer.
                    V[(opcode & 0x0F00) >> 8] = delay_timer;
                    break;
                case 0x0015: // 0xFX15 - Sets the delay timer to VX.
                    delay_timer = V[(opcode & 0x0F00) >> 8];
                    break;
                case 0x0018: // 0xFX18 - Sets the sound timer to VX.
                    sound_timer = V[(opcode & 0x0F00) >> 8];
                    break;
                case 0x0029: // 0xFX29 - Sets I to the location of the sprite for the character in VX. Characters 0-F are represented by a 4x5 font.
                    I = V[(opcode & 0x0F00) >> 8] * 5; // Each character is 5 bytes
                    break;
                case 0x0033: // 0xFX33 - Stores the binary-coded decimal representation of VX, with the hundreds digit memory location in I, then the tens digit at location I+1, and the ones digit at location I+2.
                    I = V[(opcode & 0x0F00) >> 8];
                    memory[I] = I / 100;
                    memory[I + 1] = (I / 10) % 10;
                    memory[I + 2] = I % 10;
                    break;
                case 0x0055: // 0xFX55 - Stores from V0 to VX (including VX) in memory, starting at address I. The offset from I is increased by 1 for each value written, but I itself is left unmodified.
                    for (int i = 0; i <= ((opcode & 0x0F00) >> 8); i++) {
                        memory[I + i] = V[i];
                    }
                    break;
                case 0x0065: // 0xFX65 - Fills from V0 to VX (including VX) with values from memory, starting at address I. The offset from I is increased by 1 for each value read, but I itself is left unmodified.
                    for (int i = 0; i <= ((opcode & 0x0F00) >> 8); i++) {
                        V[i] = memory[I + i];
                    }
                    break;
                default:
                    std::cerr << "Unknown opcode: " << std::hex << opcode << std::endl;
            }
        default:
            std::cerr << "Unknown opcode: " << std::hex << opcode << std::endl;
    }
    pc += 2; // Move to the next opcode

    if(delay_timer > 0)
        --delay_timer;

    if(sound_timer > 0)
    {
        if(sound_timer == 1)
            printf("Sound timer ended!\n");
            // TODO: Play sound here
        --sound_timer;
    }
}