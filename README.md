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
- Save/load `.lvl.json` (F5 / F9), test-play with TAB, back to editing with TAB
- `--selftest` round-trips the level format (CI smoke test)
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
build/Release/platformmaker.exe --selftest       # format round-trip check
```

## Controls

| Mode | Input | Action |
| --- | --- | --- |
| Editor | `1`–`6` | pick tile |
| Editor | `[` `]` / `E` | cycle entity type / toggle entity mode |
| Editor | LMB / RMB | paint / erase |
| Editor | WASD or arrows | pan camera |
| Editor | `F5` / `F9` | save / load user level |
| Editor | `TAB` | test-play the level |
| Play | arrows / `A` `D` | move |
| Play | `Space` / `W` / `Up` | jump |
| Play | `Shift` | run |
| Play | `R` / `TAB` / `Esc` | restart / back to editor |

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
- Unknown symbols load as air instead of failing, so an old build can still open
  a level made with newer packs.

Saving writes the same shape, so a level made in the editor is byte-for-byte the
kind of file you can hand-edit in any text editor or review in a Git diff.

## Business model

The game is **free** — source under MIT. **In-app purchases / add-ons** are
content packs (tile sets, entity packs, campaign chapters). Because content is
JSON behind a declarative manifest, a pack never needs a new build.

## Roadmap

- [ ] autotile for ground edges and corners
- [ ] undo/redo and area copy/paste in the editor
- [ ] level validation before publishing (spawn, goal, reachability)
- [ ] share codes (`zlib` + base64 of the JSON) and level browser
- [ ] more entities: springs, moving platforms, keys/doors, bosses
- [ ] campaign chapters 1-2 … 1-4 and a level-select screen
- [ ] Web build (Emscripten) for itch.io

## License

Code: MIT (`LICENSE`). Font: Silkscreen, SIL Open Font License 1.1
(`assets/fonts/OFL.txt`).
