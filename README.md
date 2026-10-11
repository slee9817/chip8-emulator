# CHIP-8 Emulator (C++ / SDL2)

A CHIP-8 interpreter written in C++17 with SDL2. It runs the classic 1970s–90s
CHIP-8 library — Pong, Tetris, Breakout, Space Invaders — with sound, keyboard
input, and drag-and-drop ROM loading.

![Pong running in the emulator](docs/pong.png)

## Features

- **Complete CHIP-8 instruction set** (all 35 opcodes)
- **Passes the Timendus CHIP-8 test suite** — IBM logo, Corax+ opcode test, and flags test
- **Drag-and-drop ROM loading** — drop any `.ch8` file on the window to switch games
- **Sound** — 440 Hz square wave generated through an SDL audio callback
- **Phosphor-style fade rendering** — pixels dim over a few frames instead of vanishing,
  which hides the flicker built into CHIP-8's XOR drawing (similar to how CRT phosphor did)
- **Configurable quirks** — compile-time switches for the behaviors that differ between the
  original COSMAC VIP and later interpreters (shift source register, `I` increment on load/store)
- **Fixed-rate timing** — the CPU runs at ~700 instructions/sec and the timers at 60 Hz,
  independent of frame rate

## Design

```
src/chip8.h / chip8.cpp   The virtual machine: memory, registers, stack, timers, opcodes.
                          No SDL dependency.
src/main.cpp              Platform layer: window, rendering, audio, keyboard, timing loop.
```

The emulator core knows nothing about graphics, sound, or the OS. It exposes a small
interface — `step()`, `tickTimers()`, a display buffer, a key array, and `soundOn()` — and
the platform layer drives it. That separation means the core could be unit-tested or moved
to a different frontend (terminal, browser, microcontroller) without changes.

Opcodes are decoded by their top nibble in `step()`, with the larger instruction families
(`8XYN` arithmetic, `EXNN` keys, `FXNN` misc) dispatched to their own functions.

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
- Classic games: https://github.com/kripod/chip8-roms
- Test suite: https://github.com/Timendus/chip8-test-suite

## Controls

```
CHIP-8 keypad    Keyboard
 1 2 3 C          1 2 3 4
 4 5 6 D          Q W E R
 7 8 9 E          A S D F
 A 0 B F          Z X C V
```
Esc quits. Controls vary by game — most ROM collections include a text file per game.

## Quirks

Some instructions behave differently depending on which interpreter a game was written
for. Two switches at the top of `chip8.cpp` control this:

| Constant | `true` (COSMAC VIP) | `false` (CHIP-48 / SUPER-CHIP) |
|---|---|---|
| `kShiftCopiesVY` | `8XY6`/`8XYE` copy VY into VX, then shift | Shift VX in place |
| `kLoadStoreIncrementsI` | `FX55`/`FX65` advance `I` | `I` unchanged |

If a game looks glitchy, try flipping these.

## Possible next steps

- SUPER-CHIP support (128×64 hi-res mode, scrolling, 16×16 sprites)
- Runtime quirk selection instead of compile-time constants
- Debugger panel: registers, memory view, single-step execution

## References

- Tobias V. Langhoff, *Guide to making a CHIP-8 emulator* — https://tobiasvl.github.io/blog/write-a-chip-8-emulator/
- Cowgod's Chip-8 Technical Reference — http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
- Timendus CHIP-8 test suite — https://github.com/Timendus/chip8-test-suite
