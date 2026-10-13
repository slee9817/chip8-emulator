#include "chip8.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include <random>
#include <cstdlib>

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

// SUPER-CHIP big font: digits 0-9, 10 bytes each, 8x10 pixels.
static constexpr std::array<uint8_t, 100> kBigFont = {
    0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0xC3, 0xC3, 0xE7, 0x7E, 0x3C,  // 0
    0x18, 0x38, 0x58, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C,  // 1
    0x3E, 0x7F, 0xC3, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xFF, 0xFF,  // 2
    0x3C, 0x7E, 0xC3, 0x03, 0x0E, 0x0E, 0x03, 0xC3, 0x7E, 0x3C,  // 3
    0x06, 0x0E, 0x1E, 0x36, 0x66, 0xC6, 0xFF, 0xFF, 0x06, 0x06,  // 4
    0xFF, 0xFF, 0xC0, 0xC0, 0xFC, 0xFE, 0x03, 0xC3, 0x7E, 0x3C,  // 5
    0x3E, 0x7C, 0xC0, 0xC0, 0xFC, 0xFE, 0xC3, 0xC3, 0x7E, 0x3C,  // 6
    0xFF, 0xFF, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x60, 0x60,  // 7
    0x3C, 0x7E, 0xC3, 0xC3, 0x7E, 0x7E, 0xC3, 0xC3, 0x7E, 0x3C,  // 8
    0x3C, 0x7E, 0xC3, 0xC3, 0x7F, 0x3F, 0x03, 0x03, 0x3E, 0x7C,  // 9
};


Chip8::Chip8() {
    for (size_t i = 0; i < kFont.size(); ++i) memory[kFontStart + i] = kFont[i];
    for (size_t i = 0; i < kBigFont.size(); ++i) memory[kBigFontStart + i] = kBigFont[i];
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
        if (exited) return;   // program ended with 00FD: do nothing

        // FETCH: two bytes, big-endian
        uint16_t op = (mem(pc) << 8) | mem(pc + 1);
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
        if (V[x] == nn) skip();
        break;

    case 0x4:  // 4XNN: skip next instruction if VX != NN
        if (V[x] != nn) skip();
        break;

    case 0x5: {
        int count = std::abs(x - y) + 1;   // how many registers in the range
        int dir = (x <= y) ? 1 : -1;       // walk upward (V3..V7) or downward (V7..V3)
        switch (n) {
        case 0x0:  // 5XY0: skip if VX == VY
            if (V[x] == V[y]) skip();
            break;
        case 0x2:  // 5XY2 (XO-CHIP): save VX..VY to memory at I (I unchanged)
            for (int i = 0; i < count; ++i) mem(I + i) = V[x + i * dir];
            break;
        case 0x3:  // 5XY3 (XO-CHIP): load VX..VY from memory at I (I unchanged)
            for (int i = 0; i < count; ++i) V[x + i * dir] = mem(I + i);
            break;
        default:
            unknown(op);
        }
        break;
    }

    case 0x6:  // 6XNN: VX = NN
        V[x] = nn;
        break;

    case 0x7:  // 7XNN: VX += NN (no carry flag)
        V[x] += nn;
        break;

    case 0x8: op8(op); break;

    case 0x9:  // 9XY0: skip next instruction if VX != VY
        if (V[x] != V[y]) skip();
        break;

    case 0xA:  // ANNN: I = NNN
        I = nnn;
        break;

    case 0xB:  // BNNN: jump to NNN + V0   (SUPER-CHIP quirk: XNN + VX)
        pc = nnn + (quirks.jumpUsesVX ? V[x] : V[0]);
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
    if ((op & 0xFFF0) == 0x00C0) {   // 00CN (SUPER-CHIP): scroll down N pixels
        scroll(0, op & 0x000F);
        return;
    }
    if ((op & 0xFFF0) == 0x00D0) {   // 00DN (XO-CHIP): scroll up N pixels
        scroll(0, -(op & 0x000F));
        return;
    }

    switch (op) {
    case 0x00E0:  // clear screen (XO-CHIP: only the selected planes)
        for (auto& p : display) p = static_cast<uint8_t>(p & ~planeMask);
        drawFlag = true;
        break;

    case 0x00EE:  // return from subroutine
        pc = stack[--sp];
        break;

    case 0x00FB:  // SUPER-CHIP: scroll right 4 pixels
        scroll(4, 0);
        break;

    case 0x00FC:  // SUPER-CHIP: scroll left 4 pixels
        scroll(-4, 0);
        break;

    case 0x00FD:  // SUPER-CHIP: exit
        exited = true;
        break;

    case 0x00FE:  // SUPER-CHIP: switch to low-res (64x32)
        hires = false;
        display.fill(0);
        drawFlag = true;
        break;

    case 0x00FF:  // SUPER-CHIP: switch to hi-res (128x64)
        hires = true;
        display.fill(0);
        drawFlag = true;
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
            if (quirks.shiftCopiesVY) V[x] = V[y];
            uint8_t flag = V[x] & 0x01;         // lowest bit, about to fall off
            V[x] = V[x] >> 1;
            V[0xF] = flag;                      // flag goes LAST
            break;
        }
        case 0xE: {  
            if (quirks.shiftCopiesVY) V[x] = V[y];
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
        if (keys[V[x] & 0x0F]) skip();
        break;
    case 0xA1:  // EXA1: skip if key VX is NOT pressed
        if (!keys[V[x] & 0x0F]) skip();
        break;
    default:
        unknown(op);
    }
}

void Chip8::opF(uint16_t op) {
    uint8_t x = (op >> 8) & 0x0F;
    switch (op & 0x00FF) {
    case 0x00:  // F000 NNNN (XO-CHIP): I = the 16-bit address in the next 2 bytes
        if (x == 0) {
            I = (mem(pc) << 8) | mem(pc + 1);
            pc += 2;
        }
        else {
            unknown(op);
        }
        break;
    case 0x01:  // FN01 (XO-CHIP): select drawing planes (N = 0-3)
        planeMask = x & 0x03;
        break;
    case 0x02:  // F002 (XO-CHIP): load the 16-byte audio pattern from memory at I
        if (x == 0) {
            for (int i = 0; i < 16; ++i) audioPattern[i] = mem(I + i);
            xoAudio = true;
        }
        else {
            unknown(op);
        }
        break;
    case 0x3A:  // FX3A (XO-CHIP): set the audio pitch to VX
        pitch = V[x];
        xoAudio = true;
        break;
    case 0x1E:  // FX1E: I += VX
        I += V[x];
        break;
    case 0x33: {  // FX33: store BCD of VX at I, I+1, I+2
        uint8_t value = V[x];
        mem(I) = value / 100;         // hundreds
        mem(I + 1) = (value / 10) % 10;   // tens
        mem(I + 2) = value % 10;          // ones
        break;
    }
    case 0x55:  // FX55: store V0..VX into memory starting at I
        for (int i = 0; i <= x; ++i) mem(I + i) = V[i];
        if (quirks.loadStoreIncrementsI) I += x + 1;
        break;
    case 0x65:  // FX65: load V0..VX from memory starting at I
        for (int i = 0; i <= x; ++i) V[i] = mem(I + i);
        if (quirks.loadStoreIncrementsI) I += x + 1;
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
    case 0x30:  // FX30 (SUPER-CHIP): I = address of big font digit VX
        I = kBigFontStart + (V[x] % 10) * 10;
        break;
    case 0x75:  // FX75 (SUPER-CHIP): save V0..VX to flag storage (max 8)
        for (int i = 0; i <= x && i < 8; ++i) rplFlags[i] = V[i];
        break;
    case 0x85:  // FX85 (SUPER-CHIP): load V0..VX from flag storage (max 8)
        for (int i = 0; i <= x && i < 8; ++i) V[i] = rplFlags[i];
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
// XO-CHIP: the sprite is drawn into each selected plane. If both planes are
// selected, the data for plane 2 follows right after the data for plane 1.
// Sprites are XORed onto the screen. VF = 1 if any lit pixel gets turned off.
void Chip8::draw(uint8_t xPos, uint8_t yPos, uint8_t height) {
    const int scale = hires ? 1 : 2;
    const int w = kWidth / scale;
    const int h = kHeight / scale;

    const bool big = (height == 0);
    const int rows = big ? 16 : height;
    const int cols = big ? 16 : 8;
    const int bytesPerPlane = big ? 32 : height;

    int startX = xPos % w;
    int startY = yPos % h;
    V[0xF] = 0;

    uint32_t addr = I;   // each selected plane reads the next chunk of sprite data
    for (int plane = 0; plane < 2; ++plane) {
        uint8_t bit = 1 << plane;            // plane 1 -> bit 1, plane 2 -> bit 2
        if (!(planeMask & bit)) continue;    // this plane isn't selected

        for (int row = 0; row < rows; ++row) {
            int py = startY + row;
            if (py >= h) { if (!quirks.wrapSprites) break; py %= h; }

            uint16_t bits = big ? (mem(addr + row * 2) << 8) | mem(addr + row * 2 + 1)
                : (mem(addr + row) << 8);

            for (int col = 0; col < cols; ++col) {
                int px = startX + col;
                if (px >= w) { if (!quirks.wrapSprites) break; px %= w; }

                if (bits & (0x8000 >> col)) {
                    if (flipPixel(px, py, scale, bit)) V[0xF] = 1;
                }
            }
        }
        addr += bytesPerPlane;
    }
    drawFlag = true;
}

// Flip one plane of one logical pixel. In low-res mode it covers a 2x2 block.
// Returns true if that plane was on before (a collision).
bool Chip8::flipPixel(int x, int y, int scale, uint8_t bit) {
    bool wasOn = display[(y * scale) * kWidth + (x * scale)] & bit;
    for (int dy = 0; dy < scale; ++dy)
        for (int dx = 0; dx < scale; ++dx)
            display[(y * scale + dy) * kWidth + (x * scale + dx)] ^= bit;
    return wasOn;
}

// Skip the next instruction. XO-CHIP's F000 NNNN is 4 bytes long, so skip all of it.
void Chip8::skip() {
    uint16_t next = (mem(pc) << 8) | mem(pc + 1);
    pc += (next == 0xF000) ? 4 : 2;
}

// Scroll the selected planes by (dx, dy) display pixels. Positive dx = right,
// positive dy = down. Pixels scrolled in from off-screen are blank.
void Chip8::scroll(int dx, int dy) {
    auto old = display;   // copy, so we always read the un-scrolled image
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            int sx = x - dx, sy = y - dy;   // where this pixel comes from
            uint8_t src = (sx >= 0 && sx < kWidth && sy >= 0 && sy < kHeight)
                ? old[sy * kWidth + sx] : 0;
            uint8_t& p = display[y * kWidth + x];
            // keep the unselected planes, take the selected planes from the source
            p = static_cast<uint8_t>((p & ~planeMask) | (src & planeMask));
        }
    }
    drawFlag = true;
}
void Chip8::unknown(uint16_t op) {
    std::fprintf(stderr, "Unimplemented opcode %04X at %03X\n", op, pc - 2);
}
