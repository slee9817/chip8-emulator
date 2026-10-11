// Platform layer: SDL2 window, input, timing. All emulation lives in Chip8.
#include <SDL2/SDL.h>

#include <cstdio>
#include <unordered_map>

#include "chip8.h"

#include<string>
#include<array>

static constexpr int kScale = 6;              // window = 768 x 384
static constexpr int kInstructionsPerSecond = 700;
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

// Called by SDL whenever the speakers need more audio.
// Fills the buffer with a 440 Hz square wave.
static void audioCallback(void* userdata, Uint8* stream, int len) {
    int* phase = static_cast<int*>(userdata);           // where we are in the wave
    int16_t* samples = reinterpret_cast<int16_t*>(stream);
    int count = len / 2;                                // 2 bytes per 16-bit sample

    const int period = 44100 / 440;                     // samples per wave cycle (~100)
    for (int i = 0; i < count; ++i) {
        samples[i] = (*phase < period / 2) ? 3000 : -3000;  // high half, then low half
        if (++*phase >= period) *phase = 0;
    }
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
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        Chip8::kWidth, Chip8::kHeight);

    // --- Audio: open a device that plays our square wave ---
    static int beepPhase = 0;
    SDL_AudioSpec want{};
    want.freq = 44100;               // samples per second
    want.format = AUDIO_S16SYS;      // 16-bit signed samples
    want.channels = 1;               // mono
    want.samples = 512;              // buffer size
    want.callback = audioCallback;
    want.userdata = &beepPhase;

    SDL_AudioDeviceID audioDev = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (audioDev == 0)
        std::fprintf(stderr, "No audio (%s) - continuing without sound\n", SDL_GetError());

    const double msPerInstruction = 1000.0 / kInstructionsPerSecond;
    const double msPerTimerTick = 1000.0 / kTimerHz;
    double instructionAccum = 0, timerAccum = 0, renderAccum = 0;
    std::array<float, Chip8::kWidth* Chip8::kHeight> glow{};  // brightness per pixel, 0..1
    constexpr float kFade = 0.8f;  // how much brightness a pixel keeps per frame after turning off
    Uint64 last = SDL_GetPerformanceCounter();
    bool running = true;

    while (running) {
        // --- Input ---
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
            if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                auto it = kKeyMap.find(e.key.keysym.sym);
                if (it != kKeyMap.end()) chip8.keys[it->second] = (e.type == SDL_KEYDOWN);
            }

            // NEW: a .ch8 file was dropped onto the window
            if (e.type == SDL_DROPFILE) {
                std::string path = e.drop.file;
                SDL_free(e.drop.file);           // SDL allocated this string; we must free it

                Chip8 fresh;                      // brand-new machine: cleared memory, registers, screen
                if (fresh.loadRom(path)) {
                    chip8 = fresh;                // replace the running game
                    chip8.drawFlag = true;        // redraw the (now blank) screen
                    SDL_SetWindowTitle(window, ("CHIP-8 - " + fileName(path)).c_str());
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
                if (chip8.display[i]) glow[i] = 1.0f;   // lit: full brightness
                else                  glow[i] *= kFade; // off: fade out gradually

                // blend from background (0x10) to foreground (0xE8)
                uint8_t v = static_cast<uint8_t>(0x10 + glow[i] * (0xE8 - 0x10));
                pixels[i] = 0xFF000000 | (v << 16) | (v << 8) | v;
            }

            SDL_UpdateTexture(texture, nullptr, pixels, Chip8::kWidth * sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        }

        // --- Sound: play while the sound timer is running, pause otherwise ---
        if (audioDev) SDL_PauseAudioDevice(audioDev, chip8.soundOn() ? 0 : 1);

        SDL_Delay(1);
    }

    if (audioDev) SDL_CloseAudioDevice(audioDev);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
