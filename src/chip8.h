#pragma once

#include <array>
#include <cstdint>
#include <string>

// The CHIP-8 virtual machine. Knows nothing about SDL or the OS --
// main.cpp feeds it keys, calls step(), and reads `display` to draw.
class Chip8 {
public:
    static constexpr int kWidth = 128;
    static constexpr int kHeight = 64;

    Chip8();

    bool loadRom(const std::string& path);

    // Fetch, decode and execute one instruction.
    void step();

    // Call 60 times per second.
    void tickTimers();

    // --- I/O, read/written by the platform layer ---
    std::array<uint8_t, kWidth * kHeight> display{};  // 1 = pixel on
    std::array<bool, 16> keys{};                      // true = held down
    bool drawFlag = false;                            // set when display changes
    bool exited = false;                  // SUPER-CHIP 00FD: program has ended
    // Behaviors that differ between interpreters. Can be changed at any time.
    struct Quirks {
        bool shiftCopiesVY;          // 8XY6/8XYE: copy VY into VX before shifting
        bool loadStoreIncrementsI;   // FX55/FX65: advance I past the registers
        bool jumpUsesVX;             // BNNN: jump to XNN + VX instead of NNN + V0
    };
    static constexpr Quirks kChip8Quirks = { true,  true,  false };
    static constexpr Quirks kSuperChipQuirks = { false, false, true };

    Quirks quirks = kChip8Quirks;    // start in classic CHIP-8 mode
    bool soundOn() const { return soundTimer > 0; }

private:
    std::array<uint8_t, 4096> memory{};
    std::array<uint8_t, 16> V{};          // registers V0..VF
    uint16_t I = 0;                       // index register
    uint16_t pc = 0x200;                  // program counter
    std::array<uint16_t, 16> stack{};
    uint8_t sp = 0;                       // stack pointer
    uint8_t delayTimer = 0;
    uint8_t soundTimer = 0;
    int waitingKey = -1;  // FX0A: key pressed but not yet released (-1 = none)
    bool hires = false;                   // SUPER-CHIP: 128x64 mode (false = 64x32 low-res)
    std::array<uint8_t, 8> rplFlags{};    // SUPER-CHIP FX75/FX85 storage

    static constexpr uint16_t kFontStart = 0x050;
    static constexpr uint16_t kBigFontStart = 0x0A0;

    void scrollDown(int n);
    void scrollRight(int n);
    void scrollLeft(int n);
    void op0(uint16_t op);
    void op8(uint16_t op);
    void opE(uint16_t op);
    void opF(uint16_t op);
    void draw(uint8_t x, uint8_t y, uint8_t height);
    bool flipPixel(int x, int y, int scale);
    void unknown(uint16_t op);
};
