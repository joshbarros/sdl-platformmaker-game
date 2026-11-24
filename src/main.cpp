#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "level.hpp"
#include "renderer.hpp"
#include "world.hpp"

namespace {

using pm::Align;
using pm::Camera;
using pm::Color;
using pm::Content;
using pm::Entity;
using pm::InputState;
using pm::Level;
using pm::Player;
using pm::TileDef;
using pm::World;
using namespace pm::palette;

constexpr int kWindowW = 1280;
constexpr int kWindowH = 800;
constexpr float kBarHeight = 86.0f;
constexpr float kTopBar = 44.0f;

struct Options {
    std::string shotPath;
    std::string levelPath;
    bool play = false;
    bool selftest = false;
    float autoSeconds = 0.0f;
};

struct EditorState {
    int brush = 1;
    int entityIndex = 0;
    bool entityMode = false;
    float camX = 0.0f;
    float camY = 0.0f;
    std::string status = "loaded";
    float statusTimer = 3.0f;
};

struct App {
    Content content;
    Level level;
    std::unique_ptr<World> world;
    std::string assetDir;
    std::string levelPath;
    bool playing = false;
    EditorState editor;
};

Color themeSky(const std::string& theme) {
    if (theme == "cave") return Color{0x1b, 0x16, 0x22, 255};
    if (theme == "night") return Color{0x14, 0x18, 0x2e, 255};
    return Color{0x8e, 0xc5, 0xe8, 255};
}

Color withAlpha(Color c, Uint8 a) {
    c.a = a;
    return c;
}

Color shade(Color c, float factor) {
    auto clamp8 = [](float v) { return static_cast<Uint8>(std::clamp(v, 0.0f, 255.0f)); };
    return Color{clamp8(float(c.r) * factor), clamp8(float(c.g) * factor),
                 clamp8(float(c.b) * factor), c.a};
}

void drawTile(pm::Renderer& r, const TileDef& tile, float x, float y, float size) {
    if (tile.name == "air") return;

    if (tile.hazard) {
        const Color c = tile.color;
        const float base = y + size;
        r.fillRect(x, base - 4.0f, size, 4.0f, shade(c, 0.55f));
        const int spikes = 3;
        for (int i = 0; i < spikes; ++i) {
            const float w = size / float(spikes);
            const float cx = x + w * (float(i) + 0.5f);
            r.line(cx - w * 0.45f, base - 4.0f, cx, y + 4.0f, c, 3.0f);
            r.line(cx + w * 0.45f, base - 4.0f, cx, y + 4.0f, c, 3.0f);
        }
        return;
    }

    if (tile.oneWay) {
        r.fillRect(x, y, size, size * 0.45f, tile.color);
        r.fillRect(x, y + size * 0.45f, size, 3.0f, shade(tile.color, 0.6f));
        return;
    }

    r.fillRect(x, y, size, size, tile.color);
    r.fillRect(x, y, size, 4.0f, shade(tile.color, 1.25f));
    r.fillRect(x, y + size - 4.0f, size, 4.0f, shade(tile.color, 0.72f));

    if (tile.name == "brick") {
        r.fillRect(x, y + size * 0.5f - 2.0f, size, 3.0f, shade(tile.color, 0.6f));
        r.fillRect(x + size * 0.5f - 1.5f, y, 3.0f, size * 0.5f, shade(tile.color, 0.6f));
        r.fillRect(x + size * 0.25f - 1.5f, y + size * 0.5f, 3.0f, size * 0.5f,
                   shade(tile.color, 0.6f));
        r.fillRect(x + size * 0.75f - 1.5f, y + size * 0.5f, 3.0f, size * 0.5f,
                   shade(tile.color, 0.6f));
    } else if (tile.name == "question") {
        r.frameRect(x + 3.0f, y + 3.0f, size - 6.0f, size - 6.0f, shade(tile.color, 0.45f),
                    3.0f);
        r.text("?", x + size * 0.5f, y + size * 0.5f - 12.0f, 22, shade(tile.color, 0.35f),
               Align::Center);
    } else if (tile.name == "ground") {
        r.fillRect(x, y + 4.0f, size, 3.0f, shade(tile.color, 1.45f));
        r.filledCircle(x + size * 0.3f, y + size * 0.7f, 3.0f, shade(tile.color, 0.82f));
        r.filledCircle(x + size * 0.75f, y + size * 0.45f, 2.0f, shade(tile.color, 0.82f));
    }
}

void drawBackground(pm::Renderer& r, float camX, int viewW, int viewH, const std::string& theme) {
    const Color sky = themeSky(theme);
    r.fillRect(0, 0, float(viewW), float(viewH), sky);

    const Color far = shade(sky, 0.88f);
    const Color near = shade(sky, 0.78f);
    const float groundY = float(viewH) * 0.78f;

    for (int i = 0; i < 7; ++i) {
        const float base = float(i) * 340.0f - std::fmod(camX * 0.25f, 340.0f);
        r.filledCircle(base + 120.0f, groundY + 60.0f, 190.0f, far);
    }
    for (int i = 0; i < 9; ++i) {
        const float base = float(i) * 260.0f - std::fmod(camX * 0.5f, 260.0f);
        r.filledCircle(base + 90.0f, groundY + 110.0f, 150.0f, near);
    }
    r.fillRect(0, groundY + 120.0f, float(viewW), float(viewH) - groundY - 120.0f, near);
}

void drawLevel(pm::Renderer& r, const App& app, float camX, float camY, int viewW, int viewH) {
    const Level& level = app.level;
    const int x0 = std::max(0, static_cast<int>(std::floor(camX / pm::kTile)));
    const int x1 = std::min(level.w - 1,
                            static_cast<int>(std::floor((camX + float(viewW)) / pm::kTile)));
    const int y0 = std::max(0, static_cast<int>(std::floor(camY / pm::kTile)));
    const int y1 = std::min(level.h - 1,
                            static_cast<int>(std::floor((camY + float(viewH)) / pm::kTile)));

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const int id = level.at(x, y);
            if (id <= 0) continue;
            drawTile(r, app.content.tile(id), float(x * pm::kTile) - camX,
                     float(y * pm::kTile) - camY, float(pm::kTile));
        }
    }
}

void drawEntities(pm::Renderer& r, const Content& content, const std::vector<Entity>& entities,
                    float camX, float camY) {
    for (const auto& e : entities) {
        if (!e.alive) continue;
        const pm::EntityDef* def = content.entity(e.type);
        if (!def) continue;

        const float x = e.x - camX;
        const float y = e.y - camY;
        const float w = float(def->w) * pm::kTile;
        const float h = float(def->h) * pm::kTile;

        if (e.type == "coin") {
            const float cx = x + w * 0.5f;
            const float cy = y + h * 0.5f;
            r.filledCircle(cx, cy, 11.0f, def->color);
            r.filledCircle(cx, cy, 6.0f, shade(def->color, 1.25f));
        } else if (e.type == "enemy.walker") {
            r.fillRect(x + 3.0f, y + 6.0f, w - 6.0f, h - 7.0f, def->color);
            r.fillRect(x + 7.0f, y + 12.0f, 5.0f, 7.0f, white);
            r.fillRect(x + w - 12.0f, y + 12.0f, 5.0f, 7.0f, white);
            r.fillRect(x + 3.0f, y + h - 4.0f, w - 6.0f, 4.0f, shade(def->color, 0.6f));
        } else if (e.type == "checkpoint") {
            r.fillRect(x + 4.0f, y, 4.0f, h, shade(def->color, 0.5f));
            const Color flagColor = e.touched ? shade(def->color, 1.2f) : shade(def->color, 0.75f);
            r.fillRect(x + 8.0f, y + 4.0f, w - 12.0f, h * 0.4f, flagColor);
        } else if (e.type == "goal") {
            r.fillRect(x + 4.0f, y, 5.0f, h, shade(def->color, 0.5f));
            r.fillRect(x + 9.0f, y + 4.0f, w - 13.0f, 18.0f, def->color);
            r.fillRect(x + 9.0f, y + 24.0f, w - 13.0f, 14.0f, shade(def->color, 0.65f));
            r.fillRect(x + 9.0f, y + 40.0f, w - 13.0f, 12.0f, def->color);
        } else {
            r.fillRect(x, y, w, h, def->color);
            r.frameRect(x, y, w, h, shade(def->color, 0.55f), 2.0f);
        }
    }
}

void drawPlayer(pm::Renderer& r, const Player& p, float camX, float camY) {
    const float x = p.x - camX;
    const float y = p.y - camY;
    const Color body{0xf2, 0x6d, 0x5a, 255};
    r.fillRect(x + 2.0f, y + 10.0f, p.w - 4.0f, p.h - 18.0f, body);
    r.fillRect(x + 4.0f, y + 2.0f, p.w - 8.0f, 12.0f, shade(body, 1.15f));
    const float eyeX = p.facingRight ? x + p.w - 9.0f : x + 5.0f;
    r.fillRect(eyeX, y + 5.0f, 4.0f, 5.0f, ink);
    r.fillRect(x + 2.0f, y + p.h - 14.0f, p.w - 4.0f, 4.0f, shade(body, 0.55f));
    // legs
    r.fillRect(x + 3.0f, y + p.h - 10.0f, 6.0f, 10.0f, shade(body, 0.8f));
    r.fillRect(x + p.w - 9.0f, y + p.h - 10.0f, 6.0f, 10.0f, shade(body, 0.8f));
}

void drawEditorGrid(pm::Renderer& r, const Level& level, float camX, float camY, int viewW,
                    int viewH) {
    const Color line{0xff, 0xff, 0xff, 26};
    const float top = -camY;
    const float bottom = float(level.h) * pm::kTile - camY;
    if (bottom < 0.0f || top > float(viewH)) return;

    const int x0 = std::max(0, static_cast<int>(std::floor(camX / pm::kTile)));
    const int x1 = std::min(level.w,
                            static_cast<int>(std::floor((camX + float(viewW)) / pm::kTile)) + 1);
    for (int x = x0; x <= x1; ++x) {
        const float sx = float(x * pm::kTile) - camX;
        r.fillRect(sx, std::max(top, 0.0f), 1.0f,
                   std::min(bottom, float(viewH)) - std::max(top, 0.0f), line);
    }

    const int y0 = std::max(0, static_cast<int>(std::floor(camY / pm::kTile)));
    const int y1 = std::min(level.h,
                            static_cast<int>(std::floor((camY + float(viewH)) / pm::kTile)) + 1);
    for (int y = y0; y <= y1; ++y) {
        const float sy = float(y * pm::kTile) - camY;
        r.fillRect(std::max(-camX, 0.0f), sy,
                   std::min(float(level.w) * pm::kTile - camX, float(viewW)) -
                       std::max(-camX, 0.0f),
                   1.0f, line);
    }
}

void drawPlayHud(pm::Renderer& r, const World& world, int viewW, int viewH) {
    r.fillRect(0, 0, float(viewW), 52.0f, withAlpha(Color{0x10, 0x0d, 0x18, 255}, 170));

    r.filledCircle(30.0f, 26.0f, 9.0f, Color{0xf2, 0xc9, 0x4c, 255});
    r.text(std::to_string(world.coins()), 44.0f, 15.0f, 18, white);

    r.text("deaths " + std::to_string(world.deaths()), 130.0f, 15.0f, 18,
           Color{0xd8, 0xd2, 0xe8, 255});

    char timeText[32];
    std::snprintf(timeText, sizeof(timeText), "time %0.1f", world.elapsed());
    r.text(timeText, float(viewW) - 250.0f, 15.0f, 18, Color{0xd8, 0xd2, 0xe8, 255});

    r.text("TAB edit", float(viewW) - 90.0f, 15.0f, 14, Color{0xb0, 0xa8, 0xc4, 255});

    if (world.finished()) {
        r.fillRect(0, 0, float(viewW), float(viewH), Color{0x08, 0x06, 0x0d, 200});
        r.text("LEVEL CLEAR", float(viewW) * 0.5f, float(viewH) * 0.5f - 70.0f, 52,
               accent, Align::Center);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%0.1fs   deaths %d", world.elapsed(), world.deaths());
        r.text(buf, float(viewW) * 0.5f, float(viewH) * 0.5f, 22, white, Align::Center);
        r.text("TAB to go back to the editor", float(viewW) * 0.5f,
               float(viewH) * 0.5f + 56.0f, 16, Color{0xd8, 0xd2, 0xe8, 255}, Align::Center);
    }
}

SDL_FRect paletteTileRect(int index, int viewW, int viewH) {
    const float size = 44.0f;
    const float gap = 10.0f;
    const float total = 6.0f * size + 5.0f * gap;
    const float startX = (float(viewW) - total) * 0.5f;
    return SDL_FRect{startX + float(index) * (size + gap), float(viewH) - 62.0f, size, size};
}

void drawEditorUi(pm::Renderer& r, const App& app, int viewW, int viewH,
                  const std::optional<SDL_FRect>& cursor) {
    // top bar
    r.fillRect(0, 0, float(viewW), kTopBar, Color{0x16, 0x12, 0x24, 230});
    const std::string mode = app.editor.entityMode ? "ENTITY" : "TILE";
    const std::string brushName =
        app.editor.entityMode
            ? (app.editor.entityIndex < static_cast<int>(app.content.entityOrder.size())
                   ? app.content.entityOrder[static_cast<std::size_t>(app.editor.entityIndex)]
                   : "-")
            : app.content.tile(app.editor.brush).name;
    r.text("EDITOR", 16.0f, 13.0f, 16, accent);
    r.text(app.level.title + "  (" + std::to_string(app.level.w) + "x" +
               std::to_string(app.level.h) + ")",
           130.0f, 13.0f, 16, white);
    r.text("brush " + mode + ": " + brushName, 430.0f, 13.0f, 16,
           Color{0xd8, 0xd2, 0xe8, 255});
    r.text("1-6 tile   [ ] entity   E mode   F5 save   F9 load   TAB play   ESC quit",
           float(viewW) - 16.0f, 15.0f, 12, Color{0xa8, 0x9f, 0xbd, 255}, Align::Right);

    // bottom palette
    r.fillRect(0, float(viewH) - kBarHeight, float(viewW), kBarHeight,
               Color{0x16, 0x12, 0x24, 230});

    for (int i = 0; i < static_cast<int>(app.content.tiles.size()) && i < 6; ++i) {
        const SDL_FRect box = paletteTileRect(i, viewW, viewH);
        const TileDef& tile = app.content.tiles[static_cast<std::size_t>(i)];
        const bool selected = !app.editor.entityMode && app.editor.brush == i;
        r.fillRect(box.x, box.y, box.w, box.h, Color{0x24, 0x1c, 0x38, 255});
        if (i > 0) drawTile(r, tile, box.x + 4.0f, box.y + 4.0f, box.w - 8.0f);
        r.frameRect(box.x, box.y, box.w, box.h, selected ? accent : Color{0x3a, 0x2f, 0x52, 255},
                    selected ? 3.0f : 2.0f);
        r.text(std::to_string(i + 1), box.x + 4.0f, box.y + box.h - 16.0f, 12,
               selected ? accent : Color{0x8a, 0x80, 0xa4, 255});
    }

    // entity palette
    float ex = paletteTileRect(5, viewW, viewH).x + 44.0f + 24.0f;
    r.text("entities", ex, float(viewH) - 78.0f, 12, Color{0xa8, 0x9f, 0xbd, 255});
    for (int i = 0; i < static_cast<int>(app.content.entityOrder.size()); ++i) {
        const std::string& type = app.content.entityOrder[static_cast<std::size_t>(i)];
        const pm::EntityDef* def = app.content.entity(type);
        if (!def) continue;
        const SDL_FRect box{ex, float(viewH) - 58.0f, 34.0f, 34.0f};
        const bool selected = app.editor.entityMode && app.editor.entityIndex == i;
        r.fillRect(box.x, box.y, box.w, box.h, Color{0x24, 0x1c, 0x38, 255});
        r.fillRect(box.x + 8.0f, box.y + 8.0f, 18.0f, 18.0f, def->color);
        r.frameRect(box.x, box.y, box.w, box.h,
                    selected ? accent : Color{0x3a, 0x2f, 0x52, 255},
                    selected ? 3.0f : 2.0f);
        ex += 42.0f;
    }

    if (cursor.has_value()) {
        const SDL_FRect c = *cursor;
        r.frameRect(c.x, c.y, c.w, c.h, accent, 2.0f);
        r.fillRect(c.x, c.y, c.w, 2.0f, withAlpha(accent, 120));
    }

    if (app.editor.statusTimer > 0.0f) {
        r.fillRect(float(viewW) * 0.5f - 190.0f, 60.0f, 380.0f, 40.0f,
                   Color{0x10, 0x0d, 0x18, 210});
        r.text(app.editor.status, float(viewW) * 0.5f, 70.0f, 14, white,
               Align::Center);
    }
}

std::string findAssetDir() {
    std::vector<std::string> candidates;
    if (const char* base = SDL_GetBasePath()) {
        candidates.push_back(std::string(base) + "assets/");
        candidates.push_back(std::string(base) + "../assets/");
        SDL_free(const_cast<char*>(base));
    }
    candidates.emplace_back(std::string(PM_SOURCE_ASSETS) + "/");

    for (const auto& dir : candidates) {
        if (std::filesystem::exists(dir + "tiles.json")) return dir;
    }
    throw std::runtime_error("assets/tiles.json not found next to the executable");
}

std::filesystem::path userLevelPath() {
    std::filesystem::path dir;
    if (const char* pref = SDL_GetPrefPath("platformmaker", "levels")) {
        dir = pref;
        SDL_free(const_cast<char*>(pref));
    } else {
        dir = std::filesystem::temp_directory_path();
    }
    std::filesystem::create_directories(dir);
    return dir / "user-1.lvl.json";
}

Options parseArgs(int argc, char** argv) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--shot" && i + 1 < argc) {
            opts.shotPath = argv[++i];
        } else if (arg == "--level" && i + 1 < argc) {
            opts.levelPath = argv[++i];
        } else if (arg == "--play") {
            opts.play = true;
        } else if (arg == "--selftest") {
            opts.selftest = true;
        } else if (arg == "--auto" && i + 1 < argc) {
            opts.autoSeconds = std::strtof(argv[++i], nullptr);
        }
    }
    return opts;
}

void setStatus(App& app, const std::string& text) {
    app.editor.status = text;
    app.editor.statusTimer = 3.0f;
}

void clampEditorCamera(App& app, int viewW, int viewH) {
    const float levelW = float(app.level.w) * pm::kTile;
    const float levelH = float(app.level.h) * pm::kTile;
    app.editor.camX = std::clamp(app.editor.camX, -64.0f, std::max(-64.0f, levelW - float(viewW) + 64.0f));
    if (levelH <= float(viewH)) {
        app.editor.camY = -(float(viewH) - levelH) * 0.5f;
    } else {
        app.editor.camY = std::clamp(app.editor.camY, -64.0f, levelH - float(viewH) + 64.0f);
    }
}

void paintAt(App& app, float worldX, float worldY, bool erase) {
    const int tx = static_cast<int>(std::floor(worldX / pm::kTile));
    const int ty = static_cast<int>(std::floor(worldY / pm::kTile));
    if (!app.level.inBounds(tx, ty)) return;

    if (erase) {
        app.level.set(tx, ty, 0);
        app.level.entities.erase(
            std::remove_if(app.level.entities.begin(), app.level.entities.end(),
                           [&](const Entity& e) { return e.tx == tx && e.ty == ty; }),
            app.level.entities.end());
        return;
    }

    if (app.editor.entityMode) {
        if (app.editor.entityIndex >= static_cast<int>(app.content.entityOrder.size())) return;
        const std::string& type =
            app.content.entityOrder[static_cast<std::size_t>(app.editor.entityIndex)];
        const bool duplicate = std::any_of(app.level.entities.begin(), app.level.entities.end(),
                                           [&](const Entity& e) {
                                               return e.tx == tx && e.ty == ty && e.type == type;
                                           });
        if (duplicate) return;
        Entity ent;
        ent.type = type;
        ent.tx = tx;
        ent.ty = ty;
        ent.x = float(tx) * pm::kTile;
        ent.y = float(ty) * pm::kTile;
        app.level.entities.push_back(ent);
        app.level.dirty = true;
    } else {
        app.level.set(tx, ty, app.editor.brush);
    }
}


} // namespace

int main(int argc, char** argv) {
    const Options opts = parseArgs(argc, argv);

    App app;
    try {
        app.assetDir = findAssetDir();
        app.content = Content::load(app.assetDir);

        std::string levelPath = app.assetDir + "levels/1-1.lvl.json";
        if (!opts.levelPath.empty()) levelPath = opts.levelPath;
        app.level = pm::loadLevel(levelPath, app.content);
        app.levelPath = levelPath;

        // Round-trip check of the level format: save what we loaded, load it
        // again and compare. This is the CI smoke test for UGC safety.
        if (opts.selftest) {
            const auto tmp =
                std::filesystem::temp_directory_path() / "platformmaker-selftest.lvl.json";
            const bool saved = pm::saveLevel(tmp.string(), app.level, app.content);
            Level roundTrip = pm::loadLevel(tmp.string(), app.content);
            const bool same = saved && roundTrip.w == app.level.w &&
                              roundTrip.h == app.level.h &&
                              roundTrip.tiles == app.level.tiles &&
                              roundTrip.entities.size() == app.level.entities.size() &&
                              roundTrip.startTx == app.level.startTx &&
                              roundTrip.startTy == app.level.startTy;
            std::printf("selftest: %s (%dx%d, %zu entities, %llu bytes)\n",
                        same ? "OK" : "FAIL", app.level.w, app.level.h,
                        app.level.entities.size(),
                        static_cast<unsigned long long>(
                            std::filesystem::exists(tmp) ? std::filesystem::file_size(tmp) : 0));
            return same ? 0 : 1;
        }

        pm::Renderer renderer;
        if (!renderer.init(app.assetDir + "fonts/Silkscreen-Regular.ttf", kWindowW, kWindowH)) {
            std::fprintf(stderr, "init failed: %s\n", SDL_GetError());
            return 1;
        }

        app.editor.brush = 1;
        app.editor.camX = 0.0f;
        app.editor.camY = 0.0f;

        if (opts.play) {
            app.world = std::make_unique<World>(app.level, app.content);
            app.playing = true;

            // Headless-ish smoke test: hold right and jump on a rhythm so the
            // physics runs for a while before the screenshot is taken.
            if (opts.autoSeconds > 0.0f) {
                app.world->setViewport(kWindowW, kWindowH);
                const int steps = static_cast<int>(opts.autoSeconds * 120.0f);
                for (int i = 0; i < steps; ++i) {
                    InputState in;
                    in.right = true;
                    in.run = true;
                    in.jump = (i % 48) < 12;
                    in.jumpPressed = (i % 48) == 0;
                    app.world->update(1.0f / 120.0f, in);
                }
            }
        }

        bool running = true;
        bool shotSaved = false;
        float accumulator = 0.0f;
        Uint64 previous = SDL_GetTicks();

        while (running) {
            const Uint64 now = SDL_GetTicks();
            float frameDt = float(now - previous) / 1000.0f;
            previous = now;
            frameDt = std::min(frameDt, 0.1f);
            accumulator += frameDt;

            int viewW = kWindowW;
            int viewH = kWindowH;
            SDL_GetWindowSize(renderer.window(), &viewW, &viewH);

            float mouseX = 0.0f;
            float mouseY = 0.0f;
            SDL_GetMouseState(&mouseX, &mouseY);

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                    continue;
                }
                if (event.type == SDL_EVENT_KEY_DOWN) {
                    const SDL_Keycode key = event.key.key;
                    if (key == SDLK_ESCAPE) {
                        if (app.playing) {
                            app.playing = false;
                            app.world.reset();
                        } else {
                            running = false;
                        }
                    } else if (key == SDLK_TAB) {
                        if (app.playing) {
                            app.playing = false;
                            app.world.reset();
                        } else {
                            app.world = std::make_unique<World>(app.level, app.content);
                            app.playing = true;
                            accumulator = 0.0f;
                        }
                    } else if (!app.playing) {
                        if (key >= SDLK_1 && key <= SDLK_6) {
                            app.editor.brush = static_cast<int>(key - SDLK_1);
                            app.editor.entityMode = false;
                        } else if (key == SDLK_E) {
                            app.editor.entityMode = !app.editor.entityMode;
                        } else if (key == SDLK_LEFTBRACKET) {
                            app.editor.entityMode = true;
                            const int count = static_cast<int>(app.content.entityOrder.size());
                            app.editor.entityIndex = (app.editor.entityIndex + count - 1) % count;
                        } else if (key == SDLK_RIGHTBRACKET) {
                            app.editor.entityMode = true;
                            const int count = static_cast<int>(app.content.entityOrder.size());
                            app.editor.entityIndex = (app.editor.entityIndex + 1) % count;
                        } else if (key == SDLK_F5) {
                            const auto path = userLevelPath();
                            setStatus(app, pm::saveLevel(path.string(), app.level, app.content)
                                              ? "saved " + path.string()
                                              : "save failed");
                        } else if (key == SDLK_F9) {
                            const auto path = userLevelPath();
                            if (std::filesystem::exists(path)) {
                                app.level = pm::loadLevel(path.string(), app.content);
                                setStatus(app, "loaded " + path.string());
                            } else {
                                setStatus(app, "no saved level yet");
                            }
                        }
                    } else if (key == SDLK_R) {
                        app.world->restart();
                        accumulator = 0.0f;
                    }
                }
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !app.playing) {
                    const float mx = event.button.x;
                    const float my = event.button.y;

                    if (my > float(viewH) - kBarHeight) {
                        // palette click
                        for (int i = 0; i < static_cast<int>(app.content.tiles.size()) && i < 6; ++i) {
                            const SDL_FRect box = paletteTileRect(i, viewW, viewH);
                            if (mx >= box.x && mx <= box.x + box.w && my >= box.y &&
                                my <= box.y + box.h) {
                                app.editor.brush = i;
                                app.editor.entityMode = false;
                            }
                        }
                        float ex = paletteTileRect(5, viewW, viewH).x + 44.0f + 24.0f;
                        for (int i = 0; i < static_cast<int>(app.content.entityOrder.size()); ++i) {
                            const SDL_FRect box{ex, float(viewH) - 58.0f, 34.0f, 34.0f};
                            if (mx >= box.x && mx <= box.x + box.w && my >= box.y &&
                                my <= box.y + box.h) {
                                app.editor.entityIndex = i;
                                app.editor.entityMode = true;
                            }
                            ex += 42.0f;
                        }
                        continue;
                    }

                    const float worldX = mx + app.editor.camX;
                    const float worldY = my + app.editor.camY;
                    const bool erase = event.button.button == SDL_BUTTON_RIGHT;
                    paintAt(app, worldX, worldY, erase);
                }
                if (event.type == SDL_EVENT_MOUSE_MOTION && !app.playing) {
                    if (event.motion.state & SDL_BUTTON_LMASK) {
                        if (event.motion.y < float(viewH) - kBarHeight && event.motion.y > kTopBar) {
                            paintAt(app, event.motion.x + app.editor.camX,
                                    event.motion.y + app.editor.camY, false);
                        }
                    } else if (event.motion.state & SDL_BUTTON_RMASK) {
                        if (event.motion.y < float(viewH) - kBarHeight && event.motion.y > kTopBar) {
                            paintAt(app, event.motion.x + app.editor.camX,
                                    event.motion.y + app.editor.camY, true);
                        }
                    }
                }
            }

            const bool* keys = SDL_GetKeyboardState(nullptr);

            if (app.playing) {
                InputState input;
                input.left = keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A];
                input.right = keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];
                input.jump = keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_UP] ||
                             keys[SDL_SCANCODE_W];
                input.run = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];

                static bool wasJump = false;
                input.jumpPressed = input.jump && !wasJump;
                wasJump = input.jump;

                app.world->setViewport(viewW, viewH);
                constexpr float step = 1.0f / 120.0f;
                int steps = 0;
                while (accumulator >= step && steps < 8) {
                    app.world->update(step, input);
                    accumulator -= step;
                    ++steps;
                    input.jumpPressed = false;
                }
            } else {
                const float panSpeed = 620.0f * frameDt;
                if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) app.editor.camX -= panSpeed;
                if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) app.editor.camX += panSpeed;
                if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) app.editor.camY -= panSpeed;
                if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) app.editor.camY += panSpeed;
                clampEditorCamera(app, viewW, viewH);
                app.editor.statusTimer = std::max(0.0f, app.editor.statusTimer - frameDt);
            }

            // ---- draw
            if (app.playing) {
                const Camera cam = app.world->camera();
                drawBackground(renderer, cam.x, viewW, viewH, app.level.theme);
                drawLevel(renderer, app, cam.x, cam.y, viewW, viewH);
                drawEntities(renderer, app.content, app.world->entities(), cam.x, cam.y);
                drawPlayer(renderer, app.world->player(), cam.x, cam.y);
                drawPlayHud(renderer, *app.world, viewW, viewH);
            } else {
                drawBackground(renderer, app.editor.camX, viewW, viewH, app.level.theme);
                drawLevel(renderer, app, app.editor.camX, app.editor.camY, viewW, viewH);
                drawEditorGrid(renderer, app.level, app.editor.camX, app.editor.camY, viewW,
                               viewH);
                drawEntities(renderer, app.content, app.level.entities, app.editor.camX,
                         app.editor.camY);

                Player ghost;
                ghost.x = float(app.level.startTx) * pm::kTile +
                          (pm::kTile - ghost.w) * 0.5f;
                ghost.y = float(app.level.startTy) * pm::kTile + (pm::kTile - ghost.h);
                drawPlayer(renderer, ghost, app.editor.camX, app.editor.camY);

                std::optional<SDL_FRect> cursor;
                const float wx = mouseX + app.editor.camX;
                const float wy = mouseY + app.editor.camY;
                const int tx = static_cast<int>(std::floor(wx / pm::kTile));
                const int ty = static_cast<int>(std::floor(wy / pm::kTile));
                if (mouseY > kTopBar && mouseY < float(viewH) - kBarHeight) {
                    cursor = SDL_FRect{float(tx * pm::kTile) - app.editor.camX,
                                       float(ty * pm::kTile) - app.editor.camY,
                                       float(pm::kTile), float(pm::kTile)};
                }
                drawEditorUi(renderer, app, viewW, viewH, cursor);
            }

            if (!opts.shotPath.empty() && !shotSaved) {
                // must read the backbuffer before it is presented
                renderer.screenshot(opts.shotPath);
                shotSaved = true;
                running = false;
            }

            renderer.endFrame();
        }

        renderer.shutdown();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());
        return 1;
    }
    return 0;
}
