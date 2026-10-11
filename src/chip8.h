#pragma once

#include <array>
#include <cstdint>
#include <string>

// The CHIP-8 virtual machine. Knows nothing about SDL or the OS --
// main.cpp feeds it keys, calls step(), and reads `display` to draw.
class Chip8 {
public:
    static constexpr int kWidth = 64;
    static constexpr int kHeight = 32;

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

    static constexpr uint16_t kFontStart = 0x050;

    void op0(uint16_t op);
    void op8(uint16_t op);
    void opE(uint16_t op);
    void opF(uint16_t op);
    void draw(uint8_t x, uint8_t y, uint8_t height);
    void unknown(uint16_t op);
};
