# BomberBoy

A two-player, same-keyboard Bomberman-style arena battle written in C++17 with
[SFML](https://www.sfml-dev.org/) 2.6. The arena is drawn with a small pixel-art
sprite set that ships in `assets/sprites`, and every sprite can be replaced with
your own PNGs; a shape-drawn fallback keeps the game playable without any art on
disk.

![BomberBoy in action](docs/screenshot.png)

## Features

- Local 2-player duel on a 15 x 13 grid arena that is regenerated every round.
- Bombs with a 2.4 s fuse, cross-shaped blasts and full chain reactions.
- Spawns start boxed in by crates, so the opening bomb always blows one open.
- Classic "walk off your own bomb, then it turns solid" bomb handling.
- A pulsing ring marks every live bomb, even while its owner stands on it.
- Three power-ups hidden in the destructible bricks.
- Best-of rounds (first to 3 by default) with a round-over / match-over overlay.
- 16 x 16 pixel-art sprites for tiles, bombers, bombs, flames and pickups, drawn
  at 3x zoom with nearest-neighbour scaling.
- Headless logic test suite (`--selftest`) and an offscreen screenshot mode.

## Requirements

- A C++17 compiler (GCC, Clang or MSVC)
- CMake 3.16+ *or* GNU Make
- SFML 2.5/2.6 (`graphics`, `window` and `system` modules)
- A system font for the HUD text (see [Fonts](#fonts))

Install SFML on common distributions:

```bash
sudo apt install libsfml-dev          # Debian / Ubuntu
sudo dnf install SFML-devel           # Fedora
sudo pacman -S sfml                   # Arch
brew install sfml                     # macOS
```

## Build

With CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/bomberboy
```

With the convenience Makefile:

```bash
make          # builds ./bomberboy
make run
```

## Controls

| Action            | Player 1              | Player 2                       |
| ----------------- | --------------------- | ------------------------------ |
| Move              | `W` `A` `S` `D`       | Arrow keys                     |
| Drop a bomb       | `Space`               | `Enter` or right `Ctrl`        |

Global keys: `R` restarts the round (or starts a rematch once the match is over),
`P` pauses, `Esc` quits. Player 2 drops a bomb on either `Enter` or right `Ctrl`,
so keyboards without a usable numpad key still work.

## Rules

- Each round both players spawn in opposite corners with one bomb, a one-tile
  blast and normal speed. The arena is 15 x 13 tiles with indestructible pillars
  and destructible bricks.
- A spawn keeps a three-tile pocket clear - the spawn tile plus a two-tile lane
  that leads toward the middle of the arena - and at least one crate always sits
  next to the spawn. Drop your opening bomb, step into the lane and the crate is
  gone; the second lane tile is outside the one-tile blast.
- A bomb explodes after 2.4 s. The blast runs one tile per blast range in the four
  directions, stops at walls, destroys the first brick it touches and fades after
  0.45 s. A blast that touches another bomb detonates it immediately - chains can
  clear half the arena at once.
- You can always step off the bomb you just dropped, but as soon as your body has
  fully left that tile the bomb becomes solid and blocks you (and everyone else).
- Standing in a flame eliminates you. The last player alive wins the round and
  scores a point; if both die in the same frame the round is a double knockout and
  is replayed without a score.
- The first player to win 3 rounds wins the match. Press `R` for a rematch.

![A fresh arena, seed 12](docs/arena.png)

### Power-ups

Destroyed bricks have a 28% chance to reveal a pickup. Walk over it to collect it:

| Pickup      | Look          | Effect                                |
| ----------- | ------------- | ------------------------------------- |
| Extra Bomb  | bomb icon     | +1 simultaneous bomb (1 to 8)         |
| Extra Blast | flame icon    | +1 blast range (1 to 9)               |
| Speed       | bolt icon     | +0.5 tiles/s movement (3.6 up to 6.6) |

A destroyed brick can also stay empty, so not every explosion pays out.

## Art assets

All graphics come from `assets/sprites/*.png`. The set ships with the repo, so a
normal build shows the pixel-art look immediately; the renderer searches
`assets/sprites`, `../assets/sprites` and `../../assets/sprites`, and
`BOMBERBOY_ASSETS=/some/dir` overrides the search. If the PNGs are missing the
game falls back to drawing everything with SFML shapes.

Every sprite is 16 x 16 pixels and is drawn at 48 px (3x) with nearest-neighbour
filtering, so the art stays crisp:

| File pattern                       | Sprite                                              |
| ---------------------------------- | --------------------------------------------------- |
| `floor.png` `wall.png` `brick.png` | arena tiles                                          |
| `player1_<facing>.png`             | blue bomber, `<facing>` = `down` `up` `left` `right` |
| `player2_<facing>.png`             | orange bomber                                        |
| `bomb0..2.png`                     | bomb with a flickering fuse spark                    |
| `flame_center0..1.png`             | explosion core                                       |
| `flame_h0..1.png` `flame_v0..1.png`| horizontal / vertical blast arms                    |
| `powerup_bomb/range/speed.png`     | the three pickups                                    |

To reskin the game, drop 16 x 16 PNGs with the same names into `assets/sprites`.
To edit the built-in art, change `tools/generate_assets.cpp` and regenerate:

```bash
make assets                                    # Makefile: builds and reruns the generator
cmake --build build --target generate_assets   # CMake: builds the tool
./build/generate_assets assets/sprites         # then run it yourself
```

The generator only needs SFML - no art program - which keeps the look of the game
reviewable as code.

## Command line

```
Usage: bomberboy [options]
  --selftest           run the headless logic checks and exit
  --seed <n>           build the arena from a fixed seed
  --screenshot <file>  render one frame to a PNG instead of opening a window
  --autoplay <s>       scripted play seconds before the screenshot
  -h, --help           show the help text
```

Examples:

```bash
./build/bomberboy --selftest
./build/bomberboy --seed 7
./build/bomberboy --seed 7 --autoplay 2.6 --screenshot shot.png
```

`--screenshot` renders into an offscreen `sf::RenderTexture`, so it needs an
OpenGL-capable display but never opens a window. It is handy for checking the
look of a build or for grabbing documentation images.

## Testing

The whole game logic lives in `bb::Game`, which does not depend on SFML at all.
That keeps the rules testable from a plain console program:

```bash
./build/bomberboy --selftest     # 65 checks with the shipped art, exit code 0
ctest --test-dir build --output-on-failure
```

The suite covers fresh-match state, arena generation (borders, pillars, spawn
lanes and the crate next to each spawn), border collision, bomb placement and fuse
timing, the opening bomb destroying a crate while the bomber escapes, the
ownership rules for dropped bombs, chain reactions, round resets, the match flow
and a seeded random-play stress test that keeps checking for players stuck inside
walls or bricks.

Two more checks read the PNG headers in `assets/sprites`: every sprite the
renderer asks for must be there and must be a square image. That catches renamed
or half-copied art right after a reskin. They are skipped (and the run reports
63 checks) when the art is not installed, because the shape fallback is then the
expected look. No OpenGL context is needed - only the first 24 bytes of each file
are read.

## Project layout

```
CMakeLists.txt          CMake build (also registers the CTest self-test)
Makefile                plain g++ build: make, make run, make test, make assets
src/game.hpp  game.cpp  arena, players, bombs, flames, power-ups, scoring (SFML-free)
src/renderer.*          sprite and shape drawing: tiles, bombs, flames, players, HUD
src/assets.*            sprite-sheet loading and the assets/sprites search path
src/selftest.*          the headless checks behind --selftest
src/main.cpp            window, input mapping, command line handling, demo replay
tools/generate_assets.cpp  regenerates the pixel-art PNGs in assets/sprites
assets/sprites/*.png    the 16 x 16 sprite set used by the renderer
docs/*.png              screenshots used by this README
```

## Design notes

- **Logic / rendering split.** `Game` owns every rule and only exposes read-only
  getters, so the renderer can never desync the simulation. The same class powers
  the interactive game, the self-test and the screenshot mode.
- **Input.** Movement polls the keyboard, but bomb drops are edge triggered from
  `sf::Event::KeyPressed`. A quick tap can therefore never be missed between two
  frames, and because key repeat is disabled a held key still drops only one bomb.
- **Movement.** Strictly four-directional. Inputs are advanced in six sub-steps
  per frame and the player is gently snapped towards the centre of the corridor,
  which prevents tunnelling through walls and getting caught on corners.
- **Sprites.** `SpriteSet` loads the PNGs once at start-up and the renderer only
  falls back to shapes when a sprite is missing, so the game never depends on the
  art being installed.
- **Determinism.** All randomness comes from one `std::mt19937` seeded through the
  `Game` constructor, so `--seed` plus a fixed input script reproduces a round
  exactly. That is how the screenshot mode produces its action frame.
- **Fonts.** The renderer probes a list of common system fonts (DejaVu, Liberation,
  Carlito, FreeSans, Open Sans, Cantarell, Adwaita, Arial). Set
  `BOMBERBOY_FONT=/path/to/font.ttf` to force a specific font, or drop one at
  `assets/font.ttf`. Without any font the game still runs, it just skips the text.
- **Fonts.** The renderer probes a list of common system fonts (DejaVu, Liberation,
  Carlito, FreeSans, Open Sans, Cantarell, Adwaita, Arial). Set
  `BOMBERBOY_FONT=/path/to/font.ttf` to force a specific font, or drop one at
  `assets/font.ttf`. Without any font the game still runs, it just skips the text.

## License

Personal project, no license file yet - ask before reusing it.
