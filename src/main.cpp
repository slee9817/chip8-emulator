// Platform layer: SDL2 window, input, timing. All emulation lives in Chip8.
#include <SDL2/SDL.h>

#include <cstdio>
#include <unordered_map>

#include "chip8.h"

#include<string>
#include<array>

#include <fstream>
#include <sstream>

#include <cmath>
#include <algorithm>

static constexpr int kScale = 6;              // window = 768 x 384
static constexpr int kTimerHz = 60;

// CHIP-8 keypad      Keyboard
//  1 2 3 C            1 2 3 4
//  4 5 6 D     ->     Q W E R
//  7 8 9 E            A S D F
//  A 0 B F            Z X C V
static const std::unordered_map<SDL_Keycode, int> kKeyMap = {
    {SDLK_1, 0x1}, {SDLK_2, 0x2}, {SDLK_3, 0x3}, {SDLK_4, 0xC},
    {SDLK_q, 0x4}, {SDLK_w, 0x5}, {SDLK_e, 0x6}, {SDLK_r, 0xD},
    {SDLK_a, 0x7}, {SDLK_s, 0x8}, {SDLK_d, 0x9}, {SDLK_f, 0xE},
    {SDLK_z, 0xA}, {SDLK_x, 0x0}, {SDLK_c, 0xB}, {SDLK_v, 0xF},
};

// "C:/dev/roms/games/Pong.ch8" -> "Pong.ch8"
static std::string fileName(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

// The three modes F2 cycles through: quirks plus a sensible default speed.
struct ModeInfo {
    const char* name;
    Chip8::Quirks quirks;
    int speed;            // instructions per second
};
static const ModeInfo kModes[] = {
    { "CHIP-8",     Chip8::kChip8Quirks,     700   },
    { "SUPER-CHIP", Chip8::kSuperChipQuirks, 1800  },
    { "XO-CHIP",    Chip8::kXoChipQuirks,    30000 },
};
static constexpr int kModeCount = 3;

// Window title: game name, mode, speed, and key hints.
static void updateTitle(SDL_Window* window, const std::string& romPath,
    const char* modeName, int ips) {
    std::string title = "CHIP-8 - " + fileName(romPath)
        + "   [" + modeName + " mode, " + std::to_string(ips) + " instr/s]"
        + "   F1 help | F2 mode | F3/F4 speed";
    SDL_SetWindowTitle(window, title.c_str());
}

// Look for "Game.txt" next to "Game.ch8" and return its text ("" if there isn't one).
static std::string readGameNotes(const std::string& romPath) {
    size_t dot = romPath.find_last_of('.');
    std::string txtPath = (dot == std::string::npos ? romPath : romPath.substr(0, dot)) + ".txt";

    std::ifstream file(txtPath);
    if (!file) return "";
    std::stringstream ss;
    ss << file.rdbuf();      // read the whole file at once
    return ss.str();
}

// Show the game's notes plus the keypad mapping in a pop-up window.
static void showControls(SDL_Window* window, const std::string& romPath) {
    std::string text = readGameNotes(romPath);
    if (text.empty())
        text = "No notes found for this game.\n"
        "(Looking for a .txt file with the same name as the ROM.)";

    // Game notes usually describe the CHIP-8 keypad, so show how it maps to the keyboard.
    text += "\n\nCHIP-8 keypad -> keyboard:\n"
        "  1 2 3 C   ->   1 2 3 4\n"
        "  4 5 6 D   ->   Q W E R\n"
        "  7 8 9 E   ->   A S D F\n"
        "  A 0 B F   ->   Z X C V\n\n"
        "F2: switch CHIP-8 / SUPER-CHIP mode";

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
        ("Controls - " + fileName(romPath)).c_str(),
        text.c_str(), window);
}

// Everything the audio thread needs. The main loop copies the latest values in
// while the audio device is locked, so the two threads never see half-updated data.
struct AudioState {
    std::array<uint8_t, 16> pattern{};
    uint8_t pitch = 64;
    bool xo = false;      // false = classic beep, true = play the game's pattern
    double pos = 0.0;     // current position in the 128-bit pattern
    int phase = 0;        // current position in the classic square wave
};

// Called by SDL whenever the speakers need more audio.
static void audioCallback(void* userdata, Uint8* stream, int len) {
    AudioState* a = static_cast<AudioState*>(userdata);
    int16_t* samples = reinterpret_cast<int16_t*>(stream);
    int count = len / 2;   // 2 bytes per 16-bit sample

    if (!a->xo) {
        // Classic CHIP-8 beep: 440 Hz square wave
        const int period = 44100 / 440;
        for (int i = 0; i < count; ++i) {
            samples[i] = (a->phase < period / 2) ? 3000 : -3000;
            if (++a->phase >= period) a->phase = 0;
        }
        return;
    }

    // XO-CHIP: loop the game's 128-bit pattern at the game's pitch
    double bitsPerSecond = 4000.0 * std::pow(2.0, (a->pitch - 64) / 48.0);
    double step = bitsPerSecond / 44100.0;   // how far through the pattern per output sample
    for (int i = 0; i < count; ++i) {
        int bit = static_cast<int>(a->pos);
        bool on = a->pattern[bit / 8] & (0x80 >> (bit % 8));
        samples[i] = on ? 3000 : -3000;
        a->pos += step;
        if (a->pos >= 128.0) a->pos -= 128.0;   // loop back to the start
    }
}

// XO-CHIP palette: one color per pixel value (0-3).
static constexpr uint32_t kPalette[4] = {
    0x101010,  // 0: background
    0xE8E8E8,  // 1: plane 1 only (the classic CHIP-8 color)
    0xFF7A3D,  // 2: plane 2 only
    0x4FB0FF,  // 3: both planes
};

// Mix two colors: t = 0 gives a, t = 1 gives b.
static uint32_t blend(uint32_t a, uint32_t b, float t) {
    auto channel = [&](int shift) {
        int ca = (a >> shift) & 0xFF, cb = (b >> shift) & 0xFF;
        return static_cast<uint32_t>(ca + (cb - ca) * t) << shift;
        };
    return 0xFF000000 | channel(16) | channel(8) | channel(0);
}

int main(int argc, char* argv[]) {
    // Use the ROM given on the command line, or fall back to the IBM logo test.
    std::string romPath = (argc >= 2) ? argv[1]
        : std::string(CHIP8_ROM_DIR) + "/games/Pong (1 player).ch8";

    Chip8 chip8;
    if (!chip8.loadRom(romPath)) {
        std::fprintf(stderr, "Could not load ROM: %s\n", romPath.c_str());
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        ("CHIP-8 - " + fileName(romPath)).c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        Chip8::kWidth * kScale, Chip8::kHeight * kScale, 0);
    int mode = 0;                                       // index into kModes; starts in CHIP-8
    int instructionsPerSecond = kModes[mode].speed;
    updateTitle(window, romPath, kModes[mode].name, instructionsPerSecond);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        Chip8::kWidth, Chip8::kHeight);

    // --- Audio: open a device that plays our square wave ---
    static AudioState audio;
    SDL_AudioSpec want{};
    want.freq = 44100;               // samples per second
    want.format = AUDIO_S16SYS;      // 16-bit signed samples
    want.channels = 1;               // mono
    want.samples = 512;              // buffer size
    want.callback = audioCallback;
    want.userdata = &audio;

    SDL_AudioDeviceID audioDev = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (audioDev == 0)
        std::fprintf(stderr, "No audio (%s) - continuing without sound\n", SDL_GetError());

    double msPerInstruction = 1000.0 / instructionsPerSecond;
    const double msPerTimerTick = 1000.0 / kTimerHz;
    double instructionAccum = 0, timerAccum = 0, renderAccum = 0;
    std::array<float, Chip8::kWidth* Chip8::kHeight> glow{};  // brightness per pixel, 0..1
    std::array<uint8_t, Chip8::kWidth* Chip8::kHeight> lastColor{};
    constexpr float kFade = 0.8f;  // how much brightness a pixel keeps per frame after turning off
    Uint64 last = SDL_GetPerformanceCounter();
    bool running = true;

    while (running) {
        // --- Input ---
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;

            // F1: show this game's notes and controls
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F1) {
                showControls(window, romPath);
                last = SDL_GetPerformanceCounter();   // don't "catch up" on time spent reading
            }

            // F2: cycle CHIP-8 -> SUPER-CHIP -> XO-CHIP (quirks + default speed)
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F2) {
                mode = (mode + 1) % kModeCount;
                chip8.quirks = kModes[mode].quirks;
                instructionsPerSecond = kModes[mode].speed;
                msPerInstruction = 1000.0 / instructionsPerSecond;
                updateTitle(window, romPath, kModes[mode].name, instructionsPerSecond);
            }

            // F3 / F4: halve or double the speed
            if (e.type == SDL_KEYDOWN &&
                (e.key.keysym.sym == SDLK_F3 || e.key.keysym.sym == SDLK_F4)) {
                if (e.key.keysym.sym == SDLK_F3)
                    instructionsPerSecond = std::max(100, instructionsPerSecond / 2);
                else
                    instructionsPerSecond = std::min(200000, instructionsPerSecond * 2);
                msPerInstruction = 1000.0 / instructionsPerSecond;
                updateTitle(window, romPath, kModes[mode].name, instructionsPerSecond);
            }

            // CHIP-8 keypad
            if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                auto it = kKeyMap.find(e.key.keysym.sym);
                if (it != kKeyMap.end()) chip8.keys[it->second] = (e.type == SDL_KEYDOWN);
            }

            // A .ch8 file was dropped onto the window
            if (e.type == SDL_DROPFILE) {
                std::string path = e.drop.file;
                SDL_free(e.drop.file);

                Chip8 fresh;                      // brand-new machine, in the current mode
                fresh.quirks = kModes[mode].quirks;
                if (fresh.loadRom(path)) {
                    chip8 = fresh;
                    chip8.drawFlag = true;
                    romPath = path;
                    updateTitle(window, romPath, kModes[mode].name, instructionsPerSecond);
                }
                else {
                    std::fprintf(stderr, "Could not load ROM: %s\n", path.c_str());
                }
            }
        }

        // --- Timing: run CPU and timers at fixed rates regardless of frame rate ---
        Uint64 now = SDL_GetPerformanceCounter();
        double elapsedMs = (now - last) * 1000.0 / SDL_GetPerformanceFrequency();
        last = now;
        instructionAccum += elapsedMs;
        timerAccum += elapsedMs;
        renderAccum += elapsedMs;

        if (instructionAccum > 50.0) instructionAccum = 50.0;   // never try to catch up more than 50 ms at once
        while (instructionAccum >= msPerInstruction) {
            chip8.step();
            instructionAccum -= msPerInstruction;
        }
        while (timerAccum >= msPerTimerTick) {
            chip8.tickTimers();
            timerAccum -= msPerTimerTick;
        }

        // --- Render ---
                // --- Render at 60 fps, with phosphor-style fade to hide flicker ---
        if (renderAccum >= msPerTimerTick) {
            renderAccum = 0;

            uint32_t pixels[Chip8::kWidth * Chip8::kHeight];
            for (int i = 0; i < Chip8::kWidth * Chip8::kHeight; ++i) {
                uint8_t c = chip8.display[i];
                if (c) { glow[i] = 1.0f; lastColor[i] = c; }   // lit: full brightness, remember the color
                else { glow[i] *= kFade; }                    // off: fade out gradually

                pixels[i] = blend(kPalette[0], kPalette[lastColor[i]], glow[i]);
            }

            SDL_UpdateTexture(texture, nullptr, pixels, Chip8::kWidth * sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        }

        // --- Sound: play while the sound timer is running, pause otherwise ---
                // --- Sound: hand the latest pattern/pitch to the audio thread, then play or pause ---
        if (audioDev) {
            SDL_LockAudioDevice(audioDev);      // audio thread waits while we update
            audio.pattern = chip8.audioPattern;
            audio.pitch = chip8.pitch;
            audio.xo = chip8.xoAudio;
            SDL_UnlockAudioDevice(audioDev);
            SDL_PauseAudioDevice(audioDev, chip8.soundOn() ? 0 : 1);
        }

        SDL_Delay(1);
    }

    if (audioDev) SDL_CloseAudioDevice(audioDev);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
