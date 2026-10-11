# CHIP-8 / SUPER-CHIP Emulator (C++ / SDL2)

A CHIP-8 and SUPER-CHIP interpreter written in C++17 with SDL2. It runs the classic
1970s–90s CHIP-8 library — Pong, Tetris, Breakout, Space Invaders — as well as
SUPER-CHIP games with hi-res graphics and scrolling, complete with sound, keyboard
input, and drag-and-drop ROM loading.

![Pong running in the emulator](docs/pong.png)

![Eaty the Dino, a SUPER-CHIP game, in hi-res mode](docs/eaty.png)

## Features

- **Complete CHIP-8 instruction set** (all 35 opcodes)
- **SUPER-CHIP support** — 128×64 hi-res mode, 16×16 sprites, large 8×10 font,
  scrolling (down, left, right), exit, and flag storage
- **Passes the Timendus CHIP-8 test suite** — IBM logo, Corax+ opcode test, and flags test
- **Runtime quirk switching** — press **F2** to toggle between CHIP-8 and SUPER-CHIP
  behavior while a game is running; the active mode is shown in the title bar
- **Drag-and-drop ROM loading** — drop any `.ch8` file on the window to switch games
- **Sound** — 440 Hz square wave generated through an SDL audio callback
- **Phosphor-style fade rendering** — pixels dim over a few frames instead of vanishing,
  which hides the flicker built into CHIP-8's XOR drawing (similar to how CRT phosphor did)
- **Fixed-rate timing** — the CPU runs at ~700 instructions/sec and the timers at 60 Hz,
  independent of frame rate

## Design

```
src/chip8.h / chip8.cpp   The virtual machine: memory, registers, stack, timers, opcodes.
                          No SDL dependency.
src/main.cpp              Platform layer: window, rendering, audio, keyboard, timing loop.
```

The emulator core knows nothing about graphics, sound, or the OS. It exposes a small
interface — `step()`, `tickTimers()`, a display buffer, a key array, `soundOn()`, and a
`quirks` setting — and the platform layer drives it. That separation means the core could be
unit-tested or moved to a different frontend (terminal, browser, microcontroller) without
changes.

Opcodes are decoded by their top nibble in `step()`, with the larger instruction families
(`00NN` system/display, `8XYN` arithmetic, `EXNN` keys, `FXNN` misc) dispatched to their own
functions.

The display is always stored as a 128×64 buffer. In low-res mode each CHIP-8 pixel is drawn
as a 2×2 block, so games see a 64×32 screen while the renderer always works at one size —
switching between low-res and hi-res needs no changes outside the core.

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
- Newer CHIP-8 and SUPER-CHIP games: https://johnearnest.github.io/chip8Archive/
  (look for games marked **chip8** or **schip**)
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
| F2 | Switch between CHIP-8 and SUPER-CHIP quirk modes |
| Esc | Quit |

Game controls vary — most ROM collections include a text file describing each game.

## Quirks

Some instructions behave differently depending on which interpreter a game was written
for. Press **F2** to switch between the two presets; dropping a new ROM keeps the current mode.

| Quirk | CHIP-8 mode (COSMAC VIP) | SUPER-CHIP mode |
|---|---|---|
| `shiftCopiesVY` | `8XY6`/`8XYE` copy VY into VX, then shift | Shift VX in place |
| `loadStoreIncrementsI` | `FX55`/`FX65` advance `I` | `I` unchanged |
| `jumpUsesVX` | `BNNN` jumps to NNN + V0 | Jumps to XNN + VX |

Classic games and the test ROMs expect CHIP-8 mode. SUPER-CHIP games — and many CHIP-8
games written for later interpreters — expect SUPER-CHIP mode. If a game looks glitchy,
press F2 and drop the ROM again to restart it in the other mode.

## Possible next steps

- Per-game controls pop-up, read from each game's notes file
- Saving SUPER-CHIP flag storage to disk so high scores survive restarts
- XO-CHIP support (color bitplanes, 64 KB memory, programmable audio)
- Debugger panel: registers, memory view, single-step execution

## References

- Tobias V. Langhoff, *Guide to making a CHIP-8 emulator* — https://tobiasvl.github.io/blog/write-a-chip-8-emulator/
- Cowgod's Chip-8 Technical Reference — http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
- Timendus CHIP-8 test suite — https://github.com/Timendus/chip8-test-suite
