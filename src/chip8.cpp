#include "chip8.h"
#include "SDL3/SDL.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <ctime>

// Window and rendering constants
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 320;
const int PIXEL_SIZE = 10; // Each CHIP8 pixel will be 10x10 pixels on screen

// Global variables
SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
chip8 myChip8;

// CHIP8 key mapping (hex keypad layout)
// 1 2 3 4
// Q W E R
// A S D F
// Z X C V
const SDL_Keycode keyMap[16] = {
    SDLK_1, SDLK_2, SDLK_3, SDLK_4, // 1 2 3 4
    SDLK_Q, SDLK_W, SDLK_E, SDLK_R, // Q W E R
    SDLK_A, SDLK_S, SDLK_D, SDLK_F, // A S D F
    SDLK_Z, SDLK_X, SDLK_C, SDLK_V  // Z X C V
};

bool loadROM(const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "Failed to open ROM file: " << filename << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> buffer(size); // Changed from char to unsigned char
    if (file.read(reinterpret_cast<char *>(buffer.data()), size))
    {
        size_t maxSize = 4096 - 0x200;
        size_t loadSize = (size < maxSize) ? size : maxSize;
        for (size_t i = 0; i < loadSize; ++i)
        {
            myChip8.memory[0x200 + i] = buffer[i];
        }
        std::cout << "ROM loaded successfully: " << size << " bytes" << std::endl;
        return true;
    }

    std::cerr << "Failed to read ROM file" << std::endl;
    return false;
}

bool initSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    window = SDL_CreateWindow("CHIP-8 Emulator",
                              SCREEN_WIDTH, SCREEN_HEIGHT,
                              0);
    if (!window)
    {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer)
    {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Set renderer properties
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    return true;
}

void cleanupSDL()
{
    if (renderer)
    {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_Quit();
}

void drawGraphics()
{
    // Clear the screen
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Draw the CHIP8 graphics buffer
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    for (int y = 0; y < 32; ++y)
    {
        for (int x = 0; x < 64; ++x)
        {
            if (myChip8.gfx[y * 64 + x] != 0)
            {
                // Create a rectangle for this pixel
                SDL_FRect pixelRect = {
                    (float)(x * PIXEL_SIZE),
                    (float)(y * PIXEL_SIZE),
                    (float)PIXEL_SIZE,
                    (float)PIXEL_SIZE};
                SDL_RenderFillRect(renderer, &pixelRect);
            }
        }
    }

    SDL_RenderPresent(renderer);
}

void handleInput(const SDL_Event &event)
{
    switch (event.type)
    {
    case SDL_EVENT_KEY_DOWN:
        if (event.key.key == SDLK_ESCAPE)
        {
            exit(0);
        }
        for (int i = 0; i < 16; ++i)
        {
            if (event.key.key == keyMap[i])
            {
                myChip8.set_key(i, true);
                break;
            }
        }
        break;

    case SDL_EVENT_KEY_UP:
        for (int i = 0; i < 16; ++i)
        {
            if (event.key.key == keyMap[i])
            {
                myChip8.set_key(i, false);
                break;
            }
        }
        break;
    }
}

void chip8::initialize()
{
    // Initialize the emulator state
    opcode = 0;
    I = 0;
    pc = 0x200; // The Program starts at 0x200
    sp = 0;
    // Reset timers
    delay_timer = 0;
    sound_timer = 0;

    // Clear memory
    for (unsigned char &i : memory)
    {
        i = 0;
    }

    // Clear registers
    for (unsigned char &i : V)
    {
        i = 0;
    }

    // Clear graphics buffer
    for (unsigned char &i : gfx)
    {
        i = 0;
    }

    // Clear keypad
    for (unsigned char &i : key)
    {
        i = 0;
    }

    // Clear stack
    for (unsigned char &i : stack)
    {
        i = 0;
    }

    // Initialize draw flag
    drawFlag = false;

    // Load the font set into memory
    // The font set is stored at the beginning of memory
    for (int i = 0; i < 80; i++)
    {
        memory[i] = chip8_font[i];
    }
}

void chip8::emulate_cycle()
{
    opcode = memory[pc] << 8 | memory[pc + 1]; // Fetch opcode

    switch (opcode & 0xF000)
    {
    case 0x0000:
        switch (opcode & 0x00FF)
        {
        case 0x00E0: // Clear the display
            for (unsigned char &i : gfx)
            {
                i = 0;
            }
            drawFlag = true; // Set draw flag for screen clear
            pc += 2;
            break;
        case 0x00EE: // Return from subroutine
            if (sp > 0)
            {
                --sp;
                pc = stack[sp];
            }
            else
            {
                std::cerr << "Stack underflow!" << std::endl;
                return;
            }
            break;
        default:
            std::cerr << "Unknown opcode: " << std::hex << opcode << std::endl;
            pc += 2;
        }
        break;
    case 0x1000: // 0x1NNN - Jump to address NNN
        pc = opcode & 0x0FFF;
        break;
    case 0x2000: // 0x2NNN - Call subroutine at NNN
        if (sp >= 16)
        { // Check for stack overflow
            std::cerr << "Stack overflow!" << std::endl;
            return;
        }
        stack[sp] = pc + 2;
        ++sp;
        pc = opcode & 0x0FFF;
        break;
    case 0x3000: // 0x3XNN - Skip next instruction if VX == NN
        if (V[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF))
        {
            pc += 4;
        }
        else
        {
            pc += 2;
        }
        break;
    case 0x4000: // 0x4XNN - Skip next instruction if VX != NN
        if (V[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF))
        {
            pc += 4;
        }
        else
        {
            pc += 2;
        }
        break;
    case 0x5000: // 0x5XY0 - Skip next instruction if VX == VY
        if (V[(opcode & 0x0F00) >> 8] == V[(opcode & 0x00F0) >> 4])
        {
            pc += 4;
        }
        else
        {
            pc += 2;
        }
        break;
    case 0x6000: // 0x6XNN - Set VX = NN.
        V[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
        pc += 2;
        break;
    case 0x7000: // 0x7XNN - Set VX = VX + NN.
        V[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
        pc += 2;
        break;
    case 0x8000:
        switch (opcode & 0x000F)
        {            // 0x8XY-
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
            if (V[(opcode & 0x00F0) >> 4] > (0xFF - V[(opcode & 0x0F00) >> 8]))
            {
                V[0xF] = 1;
            }
            else
            {
                V[0xF] = 0;
            }
            V[(opcode & 0x0F00) >> 8] += V[(opcode & 0x00F0) >> 4];
            break;
        case 0x0005: // VY is subtracted from VX. VF is set to 0 when there's an underflow, and 1 when there is not.
            if (V[(opcode & 0x0F00) >> 8] >= V[(opcode & 0x00F0) >> 4])
            {
                V[0xF] = 1;
            }
            else
            {
                V[0xF] = 0;
            }
            V[(opcode & 0x0F00) >> 8] -= V[(opcode & 0x00F0) >> 4];
            break;
        case 0x0006: // Shifts VX to the right by 1, then stores the least significant bit of VX prior to the shift into VF
            V[0xF] = V[(opcode & 0x0F00) >> 8] & 0x01;
            V[(opcode & 0x0F00) >> 8] >>= 1;
            break;
        case 0x0007: // Sets VX to VY - VX. VF is set to 0 when there's an underflow, and 1 when there is not.
            if (V[(opcode & 0x00F0) >> 4] >= V[(opcode & 0x0F00) >> 8])
            {
                V[0xF] = 1;
            }
            else
            {
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
            break;
        }
        pc += 2;
        break;
    case 0x9000: // 0x9XY0 - Skips next instruction if VX does not equal VY.
        if (V[(opcode & 0x0F00) >> 8] != V[(opcode & 0x00F0) >> 4])
        {
            pc += 4;
        }
        else
        {
            pc += 2;
        }
        break;
    case 0xA000: // 0xANNN - Sets I to the address NNN.
        I = opcode & 0x0FFF;
        pc += 2;
        break;
    case 0xB000: // 0xBNNN - Jumps to the address NNN plus V0.
        pc = (opcode & 0x0FFF) + V[0];
        break;
    case 0xC000: // 0xCXNN - Sets VX to the result of a bitwise and operation on a random number. (VX = rand() & NN)
        V[(opcode & 0x0F00) >> 8] = (rand() % 256) & (opcode & 0x00FF);
        pc += 2;
        break;
    case 0xD000: // 0xDXYN - Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels and height of N pixels. Each row of 8 pixels is read as bit-coded starting from memory location I; I value does not change after the execution of this instruction. As described above, VF is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn, and to 0 if that does not happen.
        V[0xF] = 0;
        for (int y = 0; y < (opcode & 0x000F); y++)
        {
            if (I + y < 4096)
            {
                unsigned char sprite_row = memory[I + y];
                for (int x = 0; x < 8; x++)
                {
                    if ((sprite_row & (0x80 >> x)) != 0)
                    {
                        int pixel_x = (V[(opcode & 0x0F00) >> 8] + x) % 64;
                        int pixel_y = (V[(opcode & 0x00F0) >> 4] + y) % 32;
                        int index = pixel_y * 64 + pixel_x;
                        if (index >= 0 && index < 64 * 32 && gfx[index] == 1)
                        {
                            V[0xF] = 1;
                        }
                        if (index >= 0 && index < 64 * 32)
                        {
                            gfx[index] ^= 1;
                        }
                    }
                }
            }
        }
        drawFlag = true;
        pc += 2;
        break;
    case 0xE000:
        switch (opcode & 0x00FF)
        {
        case 0x009E: // 0xEX9E - Skip if key pressed
            if (key[V[(opcode & 0x0F00) >> 8]] != 0)
            {
                pc += 4;
            }
            else
            {
                pc += 2;
            }
            break;
        case 0x00A1: // 0xEXA1 - Skips the next instruction if the key stored in VX is not pressed.
            if (key[V[(opcode & 0x0F00) >> 8]] == 0)
            {
                pc += 4;
            }
            else
            {
                pc += 2;
            }
            break;
        default:
            std::cerr << "Unknown opcode: " << std::hex << opcode << std::endl;
            pc += 2;
            break;
        }
        break;
    case 0xF000:
        switch (opcode & 0x00FF)
        {
        case 0x0007: // 0xFX07 - Sets VX to the value of the delay timer.
            V[(opcode & 0x0F00) >> 8] = delay_timer;
            pc += 2;
            break;
        case 0x000A:
        { // 0xFX0A - A key press is awaited, and then stored in VX (blocking).
            bool keyPressed = false;
            for (int i = 0; i < 16; i++)
            {
                if (key[i] != 0)
                {
                    V[(opcode & 0x0F00) >> 8] = i;
                    keyPressed = true;
                    break;
                }
            }
            if (!keyPressed)
            {
                return;
            }
            pc += 2;
            break;
        }
        case 0x0015:
        {
            // 0xFX15 - Sets the delay timer to VX.
            delay_timer = V[(opcode & 0x0F00) >> 8];
            pc += 2;
            break;
        }
        case 0x0018:
        { // 0xFX18 - Sets the sound timer to VX.
            sound_timer = V[(opcode & 0x0F00) >> 8];
            pc += 2;
            break;
        }
        case 0x001E:
        {
            // 0xFX1E - Adds VX to I.
            I += V[(opcode & 0x0F00) >> 8];
            pc += 2;
            break;
        }
        case 0x0029:
        {
            // 0xFX29 - Sets I to the location of the sprite for the character in VX. Characters 0-F are represented by a 4x5 font.
            I = V[(opcode & 0x0F00) >> 8] * 5;
            pc += 2;
            break;
        }
        case 0x0033:
        {
            // 0xFX33 - Stores the binary-coded decimal representation of VX, with the hundreds digit memory location in I, then the tens digit at location I+1, and the ones digit at location I+2.
            unsigned char value = V[(opcode & 0x0F00) >> 8];
            if (I < 4093)
            {
                memory[I] = value / 100;
                memory[I + 1] = (value / 10) % 10;
                memory[I + 2] = value % 10;
            }
            else
            {
                std::cerr << "Memory access out of bounds!" << std::endl;
            }
            pc += 2;
            break;
        }
        case 0x0055:
        { // 0xFX55 - Stores from V0 to VX (including VX) in memory, starting at address I. The offset from I is increased by 1 for each value written, but I itself is left unmodified.
            int x = (opcode & 0x0F00) >> 8;
            if (I + x < 4096)
            {
                for (int i = 0; i <= x; i++)
                {
                    memory[I + i] = V[i];
                }
                // I = I + x + 1; // Add this line for legacy compatibility
            }
            else
            {
                std::cerr << "Memory access out of bounds!" << std::endl;
            }
            pc += 2;
            break;
        }
        case 0x0065:
        {
            // 0xFX65 - Fills from V0 to VX (including VX) with values from memory, starting at address I. The offset from I is increased by 1 for each value read, but I itself is left unmodified.
            int x = (opcode & 0x0F00) >> 8;
            if (I + x < 4096)
            {
                for (int i = 0; i <= x; i++)
                {
                    V[i] = memory[I + i];
                }
                // I = I + x + 1; // Add this line for legacy compatibility
            }
            else
            {
                std::cerr << "Memory access out of bounds!" << std::endl;
            }
            pc += 2;
            break;
        }
        default:
            std::cerr << "Unknown opcode: " << std::hex << opcode << std::endl;
            pc += 2;
            break;
        }
    default:
        std::cerr << "Unknown opcode: " << std::hex << opcode << std::endl;
        pc += 2;
        break;
    }

    if (delay_timer > 0)
        --delay_timer;

    if (sound_timer > 0)
    {
        if (sound_timer == 1)
            printf("Sound timer ended!\n");
        --sound_timer;
    }
}

void chip8::set_key(int key_index, bool pressed)
{
    if (key_index >= 0 && key_index < 16)
    {
        key[key_index] = pressed ? 1 : 0; // Set the key state
    }
    else
    {
        std::cerr << "Invalid key index: " << key_index << std::endl;
    }
}

int main(int argc, char *argv[])
{
    std::cout << "CHIP-8 Emulator Starting..." << std::endl;

    // Initialize random seed
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // Initialize CHIP8
    myChip8.initialize();

    // Initialize SDL
    if (!initSDL())
    {
        return 1;
    }

    // Load ROM if provided
    if (argc > 1)
    {
        if (!loadROM(argv[1]))
        {
            std::cerr << "Usage: " << argv[0] << " [rom_file]" << std::endl;
            cleanupSDL();
            return 1;
        }
    }
    else
    {
        std::cout << "No ROM file provided. Running with empty memory." << std::endl;
        std::cout << "Usage: " << argv[0] << " [rom_file]" << std::endl;
    }

    // Main emulation loop
    const int targetFPS = 60;
    const int frameDelay = 1000 / targetFPS;
    const int cyclesPerFrame = 10; // Instructions to execute per frame. Adjust this for speed.

    Uint32 frameStart;
    int frameTime;
    bool quit = false;

    std::cout << "Emulation started. Press ESC to quit." << std::endl;

    while (!quit) // Use a flag instead of while(true)
    {
        frameStart = SDL_GetTicks();

        // Use a more robust event handling loop
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                quit = true;
            }
            // Pass the event to your input handler
            handleInput(event); // Modify handleInput to take an SDL_Event
        }

        // --- Execute multiple CPU cycles per frame ---
        for (int i = 0; i < cyclesPerFrame; ++i)
        {
            myChip8.emulate_cycle();
        }

        // Update timers (which happens inside emulate_cycle at 60Hz)
        // and draw the screen if needed.
        if (myChip8.drawFlag)
        {
            drawGraphics();
            myChip8.drawFlag = false;
        }

        // Frame rate limiting
        frameTime = SDL_GetTicks() - frameStart;
        if (frameDelay > frameTime)
        {
            SDL_Delay(frameDelay - frameTime);
        }
    }

    cleanupSDL();
    return 0;
}