#include "chip8.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include <random>

// Built-in hex font: 16 characters (0-F), 5 bytes each, 4 pixels wide.
static constexpr std::array<uint8_t, 80> kFont = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
    0x20, 0x60, 0x20, 0x20, 0x70,  // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
    0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
    0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
    0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
    0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
    0xF0, 0x80, 0xF0, 0x80, 0x80,  // F
};
// Quirk: original COSMAC VIP copies VY into VX before shifting.
// Set to false for games written for later interpreters (CHIP-48 / SUPER-CHIP).
static constexpr bool kShiftCopiesVY = true;
// Quirk: original COSMAC VIP leaves I pointing past the last register after FX55/FX65.
static constexpr bool kLoadStoreIncrementsI = true;

Chip8::Chip8() {
    for (size_t i = 0; i < kFont.size(); ++i) memory[kFontStart + i] = kFont[i];
}

bool Chip8::loadRom(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    if (data.size() > memory.size() - 0x200) {
        std::fprintf(stderr, "ROM too large (%zu bytes)\n", data.size());
        return false;
    }
    for (size_t i = 0; i < data.size(); ++i) memory[0x200 + i] = data[i];
    return true;
}

void Chip8::tickTimers() {
    if (delayTimer > 0) --delayTimer;
    if (soundTimer > 0) --soundTimer;
}

void Chip8::step() {
    // FETCH: two bytes, big-endian
    uint16_t op = (memory[pc] << 8) | memory[pc + 1];
    pc += 2;

    // DECODE: pull out the common fields once
    uint8_t  x   = (op >> 8) & 0x0F;   // second nibble
    uint8_t  y   = (op >> 4) & 0x0F;   // third nibble
    uint8_t  n   = op & 0x000F;        // fourth nibble
    uint8_t  nn  = op & 0x00FF;        // second byte
    uint16_t nnn = op & 0x0FFF;        // address

    // EXECUTE
    switch (op >> 12) {
    case 0x0: op0(op); break;

    case 0x1:  // 1NNN: jump to NNN
        pc = nnn;
        break;

    case 0x2:  // 2NNN: call subroutine at NNN
        stack[sp++] = pc;
        pc = nnn;
        break;

    case 0x3:  // 3XNN: skip next instruction if VX == NN
        if (V[x] == nn) pc += 2;
        break;

    case 0x4:  // 4XNN: skip next instruction if VX != NN
        if (V[x] != nn) pc += 2;
        break;

    case 0x5:  // 5XY0: skip next instruction if VX == VY
        if (V[x] == V[y]) pc += 2;
        break;

    case 0x6:  // 6XNN: VX = NN
        V[x] = nn;
        break;

    case 0x7:  // 7XNN: VX += NN (no carry flag)
        V[x] += nn;
        break;

    case 0x8: op8(op); break;

    case 0x9:  // 9XY0: skip next instruction if VX != VY
        if (V[x] != V[y]) pc += 2;
        break;

    case 0xA:  // ANNN: I = NNN
        I = nnn;
        break;

    case 0xB:  // BNNN: jump to NNN + V0
        pc = nnn + V[0];
        break;

    case 0xC: { // CXNN: VX = random byte & NN
        static std::mt19937 rng{ std::random_device{}() };
        V[x] = (rng() & 0xFF) & nn;
        break;
    }

        case 0xD:  // DXYN: draw sprite
            draw(V[x], V[y], n);
            break;

        case 0xE: opE(op); break;
        case 0xF: opF(op); break;
    }

}

void Chip8::op0(uint16_t op) {
    switch (op) {
        case 0x00E0:  // clear screen
            display.fill(0);
            drawFlag = true;
            break;

        case 0x00EE:  // return from subroutine
            pc = stack[--sp];
            break;

        default:      // 0NNN: machine code routine -- ignored by modern emulators
            break;
    }
}

void Chip8::op8(uint16_t op) {
    uint8_t x = (op >> 8) & 0x0F;
    uint8_t y = (op >> 4) & 0x0F;

    switch (op & 0x000F) {
        // TODO: implement all of these. Remember: compute the flag first,
        // write VX, THEN write VF (in case X is F).
        case 0x0:  // 8XY0: VX = VY
            V[x] = V[y];
            break;
        case 0x1:  // 8XY1: VX |= VY
            V[x] |= V[y];
            break;
        case 0x2:  // 8XY2: VX &= VY
            V[x] &= V[y];
            break;
        case 0x3:  // 8XY3: VX ^= VY
            V[x] ^= V[y];
            break;
        case 0x4: {  // 8XY4: VX += VY, VF = carry
            uint16_t sum = V[x] + V[y];
            uint8_t carry = (sum > 255) ? 1 : 0;
            V[x] = sum & 0xFF;
            V[0xF] = carry;
            break;
        }
        case 0x5: {  // 8XY5: VX -= VY, VF = 1 if no borrow (VX >= VY)
            uint8_t flag = (V[x] >= V[y]) ? 1 : 0;
            V[x] = V[x] - V[y];
            V[0xF] = flag;
            break;
        }
        case 0x7: {  // 8XY7: VX = VY - VX, VF = 1 if no borrow (VY >= VX)
            uint8_t flag = (V[y] >= V[x]) ? 1 : 0;
            V[x] = V[y] - V[x];
            V[0xF] = flag;
            break;
        }
        case 0x6: {  // 8XY6: VX >>= 1, VF = bit shifted out
            if (kShiftCopiesVY) V[x] = V[y];
            uint8_t flag = V[x] & 0x01;         // lowest bit, about to fall off
            V[x] = V[x] >> 1;
            V[0xF] = flag;                      // flag goes LAST
            break;
        }
        case 0xE: {  
            if (kShiftCopiesVY) V[x] = V[y];
            uint8_t flag = (V[x] >> 7) & 0x01;  
            V[x] = V[x] << 1;
            V[0xF] = flag;                      
            break;
        }
        default:
            unknown(op);
    }
}

void Chip8::opE(uint16_t op) {
    uint8_t x = (op >> 8) & 0x0F;
    switch (op & 0x00FF) {
        
    case 0x9E:  // EX9E: skip if key VX is pressed
        if (keys[V[x] & 0x0F]) pc += 2;
        break;
    case 0xA1:  // EXA1: skip if key VX is NOT pressed
        if (!keys[V[x] & 0x0F]) pc += 2;
        break;
    default:
        unknown(op);
    }
}

void Chip8::opF(uint16_t op) {
    uint8_t x = (op >> 8) & 0x0F;
    switch (op & 0x00FF) {
    case 0x1E:  // FX1E: I += VX
        I += V[x];
        break;
    case 0x33: {  // FX33: store BCD of VX at I, I+1, I+2
        uint8_t value = V[x];
        memory[I] = value / 100;         // hundreds
        memory[I + 1] = (value / 10) % 10;   // tens
        memory[I + 2] = value % 10;          // ones
        break;
    }
    case 0x55:  // FX55: store V0..VX into memory starting at I
        for (int i = 0; i <= x; ++i) memory[I + i] = V[i];
        if (kLoadStoreIncrementsI) I += x + 1;
        break;
    case 0x65:  // FX65: load V0..VX from memory starting at I
        for (int i = 0; i <= x; ++i) V[i] = memory[I + i];
        if (kLoadStoreIncrementsI) I += x + 1;
        break;
    case 0x07:  // FX07: VX = delay timer
        V[x] = delayTimer;
        break;
    case 0x15:  // FX15: delay timer = VX
        delayTimer = V[x];
        break;
    case 0x18:  // FX18: sound timer = VX
        soundTimer = V[x];
        break;
    case 0x29:  // FX29: I = address of font sprite for digit VX
        I = kFontStart + (V[x] & 0x0F) * 5;
        break;
    case 0x0A: {  // FX0A: wait for a key press + release, store it in VX
        if (waitingKey == -1) {
            // Phase 1: look for any key going down
            for (int k = 0; k < 16; ++k) {
                if (keys[k]) { waitingKey = k; break; }
            }
            pc -= 2;  // repeat this instruction next step
        }
        else if (keys[waitingKey]) {
            pc -= 2;  // Phase 2: key still held, keep waiting for release
        }
        else {
            V[x] = waitingKey;  // key released: done
            waitingKey = -1;
        }
        break;
    }

        // Not implemented yet -- these must stay grouped right above default
    default:
        unknown(op);
    }
}


// DXYN: draw an N-byte sprite from memory[I] at (VX, VY).
// DXY0 (SUPER-CHIP): draw a 16x16 sprite, 2 bytes per row.
// Sprites are XORed onto the screen. VF = 1 if any lit pixel gets turned off.
void Chip8::draw(uint8_t xPos, uint8_t yPos, uint8_t height) {
    // In low-res mode the game sees a 64x32 screen, and each of its pixels
    // covers a 2x2 block of our 128x64 display.
    const int scale = hires ? 1 : 2;
    const int w = kWidth / scale;
    const int h = kHeight / scale;

    const bool big = (height == 0);
    const int rows = big ? 16 : height;
    const int cols = big ? 16 : 8;

    int startX = xPos % w;   // starting position wraps...
    int startY = yPos % h;
    V[0xF] = 0;

    for (int row = 0; row < rows; ++row) {
        int py = startY + row;
        if (py >= h) break;  // ...but the sprite itself clips at the edge

        // Each row as 16 bits, left-aligned: big sprites use 2 bytes,
        // normal sprites put their 1 byte in the top half.
        uint16_t bits = big ? (memory[I + row * 2] << 8) | memory[I + row * 2 + 1]
            : (memory[I + row] << 8);

        for (int col = 0; col < cols; ++col) {
            int px = startX + col;
            if (px >= w) break;

            if (bits & (0x8000 >> col)) {
                if (flipPixel(px, py, scale)) V[0xF] = 1;
            }
        }
    }
    drawFlag = true;
}

// Flip one logical pixel. In low-res mode it covers a 2x2 block of the display.
// Returns true if the pixel was on before (a collision).
bool Chip8::flipPixel(int x, int y, int scale) {
    bool wasOn = display[(y * scale) * kWidth + (x * scale)];
    for (int dy = 0; dy < scale; ++dy)
        for (int dx = 0; dx < scale; ++dx)
            display[(y * scale + dy) * kWidth + (x * scale + dx)] ^= 1;
    return wasOn;
}

void Chip8::unknown(uint16_t op) {
    std::fprintf(stderr, "Unimplemented opcode %04X at %03X\n", op, pc - 2);
}
