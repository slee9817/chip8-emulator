# CHIP-8 / SUPER-CHIP / XO-CHIP Emulator (C++ / SDL2)

An emulator for the CHIP-8 family of virtual machines, written in C++17 with SDL2. It runs
the classic 1970s–90s CHIP-8 library (Pong, Tetris, Breakout, Space Invaders), SUPER-CHIP
games with hi-res graphics and scrolling, and modern XO-CHIP games with 4-color graphics,
64 KB of memory, and programmable sound.

![Pong running in the emulator](docs/pong.png)

![Eaty the Dino, a SUPER-CHIP game, in hi-res mode](docs/eaty.png)

![Red October, an XO-CHIP game, in color](docs/redoctober.png)

## Features

- **Three platforms in one emulator**
  - **CHIP-8** — complete instruction set (all 35 opcodes)
  - **SUPER-CHIP** — 128×64 hi-res mode, 16×16 sprites, large 8×10 font, scrolling,
    exit, and flag storage
  - **XO-CHIP** — two bitplanes for 4-color graphics, 64 KB memory, the 4-byte
    `F000 NNNN` long-address instruction, register-range save/load, scroll up,
    and programmable audio (128-bit sample patterns with adjustable pitch)
- **Passes the Timendus CHIP-8 test suite** — IBM logo, Corax+ opcode test, and flags test
- **Mode switching at runtime** — **F2** cycles CHIP-8 → SUPER-CHIP → XO-CHIP; each mode
  sets the correct quirk behavior and a sensible default speed
- **Adjustable speed** — **F3 / F4** halve or double the instructions per second
- **Per-game help** — **F1** shows the current game's notes and the keypad mapping,
  read from a `.txt` file next to the ROM
- **Drag-and-drop ROM loading** — drop any `.ch8` file on the window to switch games
- **Sound** — classic 440 Hz beep for CHIP-8 games; XO-CHIP sample-pattern playback
  generated in an SDL audio callback, with thread-safe handoff from the emulator core
- **Phosphor-style fade rendering** — pixels fade over a few frames instead of vanishing,
  which hides the flicker built into CHIP-8's XOR drawing (similar to CRT phosphor)
- **Safe memory access** — all addresses wrap within 64 KB instead of reading out of bounds

## Design

```
src/chip8.h / chip8.cpp   The virtual machine: memory, registers, stack, timers, display
                          planes, audio state, and all opcodes. No SDL dependency.
src/main.cpp              Platform layer: window, color rendering, audio playback,
                          keyboard, mode/speed controls, and the timing loop.
```

The emulator core knows nothing about graphics, sound, or the OS. It exposes a small
interface — `step()`, `tickTimers()`, a display buffer, a key array, audio state, and a
`quirks` setting — and the platform layer drives it. That separation means the core could
be unit-tested or moved to a different frontend (terminal, browser, microcontroller)
without changes.

**Display.** The screen is always stored as a 128×64 buffer, one byte per pixel. Each
byte holds two bits — one per XO-CHIP bitplane — giving four colors. In low-res mode each
game pixel is drawn as a 2×2 block, so games see a 64×32 screen while the renderer always
works at one size. Classic CHIP-8 and SUPER-CHIP games only ever use plane 1, so they run
unchanged.

**Decoding.** Opcodes are dispatched by their top nibble in `step()`, with the larger
families (`00NN` system/display, `5XYN` compare/range, `8XYN` arithmetic, `EXNN` keys,
`FXNN` misc) handled in their own functions.

**Timing.** The CPU and the 60 Hz timers run on separate fixed-rate accumulators,
independent of frame rate, with a cap so a slow frame can't cause a runaway catch-up.

## Building

Requires CMake 3.16+, a C++17 compiler, and SDL2.

**Windows (Visual Studio + vcpkg)**
```
vcpkg install sdl2:x64-windows
vcpkg integrate install
```
Then open the folder in Visual Studio (File → Open → Folder) and build.

**macOS**
```
brew install sdl2 cmake
cmake -B build && cmake --build build
```

**Linux**
```
sudo apt install libsdl2-dev cmake g++
cmake -B build && cmake --build build
```

## Running

```
chip8 path/to/game.ch8
```
Or drag a `.ch8` file onto the window while it's running.

ROMs aren't included in this repo. Good sources:
- Classic CHIP-8 games: https://github.com/kripod/chip8-roms
- Newer CHIP-8, SUPER-CHIP, and XO-CHIP games: https://johnearnest.github.io/chip8Archive/
- Test suite: https://github.com/Timendus/chip8-test-suite

## Controls

```
CHIP-8 keypad    Keyboard
 1 2 3 C          1 2 3 4
 4 5 6 D          Q W E R
 7 8 9 E          A S D F
 A 0 B F          Z X C V
```

| Key | Action |
|---|---|
| F1 | Show the current game's notes and controls |
| F2 | Cycle CHIP-8 / SUPER-CHIP / XO-CHIP mode |
| F3 / F4 | Halve / double emulation speed |
| Esc | Quit |

## Modes and quirks

Some instructions behave differently depending on which interpreter a game was written
for, and newer games expect to run much faster. Each mode sets both:

| | CHIP-8 | SUPER-CHIP | XO-CHIP |
|---|---|---|---|
| `8XY6`/`8XYE` shift | copies VY into VX first | shifts VX in place | copies VY into VX first |
| `FX55`/`FX65` | advance `I` | `I` unchanged | advance `I` |
| `BNNN` jump | NNN + V0 | XNN + VX | NNN + V0 |
| Sprites at screen edge | clipped | clipped | wrap around |
| Default speed | 700 instr/s | 1,800 instr/s | 30,000 instr/s |

The active mode and speed are shown in the title bar. Dropping a new ROM keeps the current
mode. If a game looks glitchy, press F2 to try another mode and drop the ROM again to
restart it; if it runs too fast or slow, adjust with F3 / F4.

## Possible next steps

- Saving SUPER-CHIP flag storage to disk so high scores survive restarts
- Per-game settings (mode and speed) loaded automatically from the ROM's notes
- A debugger panel: registers, memory view, single-step execution
- A WebAssembly build so the emulator runs in a browser

## References

- Tobias V. Langhoff, *Guide to making a CHIP-8 emulator* — https://tobiasvl.github.io/blog/write-a-chip-8-emulator/
- Cowgod's Chip-8 Technical Reference — http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
- John Earnest, *XO-CHIP specification* — https://johnearnest.github.io/Octo/docs/XO-ChipSpecification.html
- Timendus CHIP-8 test suite — https://github.com/Timendus/chip8-test-suite
