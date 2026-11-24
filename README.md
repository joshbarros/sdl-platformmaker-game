# PLATFORMMAKER (working title)

A tile platformer **and** its level editor in the same binary, written from
scratch in C++20 on SDL3. The campaign ships as plain `.lvl.json` files — the
same format players use — so every official level doubles as a tutorial and as
a test of the loader.

The goal is user-generated content as a first-class feature, not a bolt-on: the
engine has no hardcoded level, no hardcoded tiles, and no hardcoded entities.

## What already works (M0 + M2)

- Tile collision, variable jump height, coyote time, jump buffering, run speed
- Camera with deadzone and level clamping, parallax hills
- Walkers (patrol, stomp, ledge detection), coins, checkpoints, goal
- In-game editor: paint tiles, place entities, erase, free camera, grid snapping
- **Share codes**: a whole level as text (`PM1-…`), 158 characters for the shipped 64x15 level
- **Replays**: an input log that reproduces a run exactly (deterministic simulation)
- Save/load `.lvl.json` (F5 / F9), test-play with TAB, back to editing with TAB
- `--selftest` round-trips the level format **and** replay determinism (CI smoke test)
- `--auto <seconds>` drives the simulation before a screenshot (visual smoke test)

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Dependencies (SDL3, SDL3_ttf, nlohmann/json) are fetched by CMake. Windows:

```sh
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Run

```sh
build/Release/platformmaker.exe                  # editor, campaign level 1-1
build/Release/platformmaker.exe --play           # straight into play mode
build/Release/platformmaker.exe --shot a.bmp     # render one frame and quit
build/Release/platformmaker.exe --auto 6 --play --shot a.bmp
build/Release/platformmaker.exe --selftest       # format + replay checks
```

## Controls

| Mode | Input | Action |
| --- | --- | --- |
| Editor | `1`–`6` | pick tile |
| Editor | `[` `]` / `E` | cycle entity type / toggle entity mode |
| Editor | LMB / RMB | paint / erase |
| Editor | WASD or arrows | pan camera |
| Editor | `Ctrl+C` / `Ctrl+V` | copy / paste share code |
| Editor | `F5` / `F9` | save / load user level |
| Editor | `TAB` | test-play the level |
| Play | arrows / `A` `D` | move |
| Play | `Space` / `W` / `Up` | jump |
| Play | `Shift` | run |
| Play | `R` / `TAB` / `Esc` | restart / back to editor |
| Play | automatic | the run is recorded and saved when you leave play mode |

## Sharing a level

`Ctrl+C` in the editor puts a code like this on the clipboard:

```text
PM1-UE0BQAAPAAIADAAAAP__FAMFAEcEAQApAwUADQIDAAMCAwCNBQMADQEUAAMBEQADASkAAwERAAMBFQsAAAoACQAACwAJAAAMAAkAABQABwAAFQAHAAEQAAwAAR4ADAACGAALAAEuAAwAACwACgADPAAKAA
```

158 characters for the shipped level. It is `PM1-` plus base64url of a packed
payload: size, start, theme, run-length-encoded tiles and entity placements. No
server, no JSON inside the code, and no compression library — so it fits in a
caption or a chat message, and anyone can paste it back with `Ctrl+V`.

CLI equivalents:

```sh
platformmaker --share                      # prints the code for the loaded level
platformmaker --from-share PM1-... --play  # loads a code and starts playing
```

## Replays

While you play, every input is recorded (4 bits per fixed step). Leaving play
mode saves the run to `%APPDATA%/platformmaker/replays/last.replay.json`.

Because the simulation is fixed-step with no randomness, replaying that log
reproduces the run exactly — same position, coins and deaths. That is what makes
ghosts, verified records and auto-generated clips possible later.

```sh
platformmaker --replay path/to/last.replay.json
```

## Level format

Human-editable JSON, with the map as ASCII rows:

```json
{
  "format": 1,
  "id": "campaign/1-1",
  "title": "Green Hills",
  "theme": "grass",
  "size": { "w": 64, "h": 15 },
  "start": { "x": 2, "y": 12 },
  "rows": [
    "................................................................",
    "..................=====.........................................",
    "####################...#################...#####################"
  ],
  "entities": [
    { "type": "enemy.walker", "x": 16, "y": 12 },
    { "type": "goal", "x": 60, "y": 10 }
  ]
}
```

- `assets/tiles.json` maps symbols to tile behaviour (`solid`, `oneWay`, `hazard`) and color.
- `assets/entities.json` declares entity types, sizes and colors.
- Unknown symbols load as air instead of failing, so an older build can still
  open a level made with newer packs.

Saving writes the same shape, so a level made in the editor is the kind of file
you can hand-edit in any text editor or review in a Git diff.

## Business model

The game is **free** — source under MIT. **In-app purchases / add-ons** are
content packs (tile sets, entity packs, campaign chapters). Because content is
JSON behind a declarative manifest, a pack never needs a new build.

## Roadmap

- [ ] autotile for ground edges and corners
- [ ] undo/redo and area copy/paste in the editor
- [x] share codes (packed binary + base64url)
- [ ] level browser / optional server for discovery
- [ ] ghost overlay of your best replay while playing
- [ ] clip export (9:16 frames + GIF) with the code burned in
- [ ] level validation before publishing (spawn, goal, reachability)
- [ ] more entities: springs, moving platforms, keys/doors, bosses
- [ ] campaign chapters 1-2 … 1-4 and a level-select screen
- [ ] Web build (Emscripten) for itch.io

## License

Code: MIT (`LICENSE`). Font: Silkscreen, SIL Open Font License 1.1
(`assets/fonts/OFL.txt`).
