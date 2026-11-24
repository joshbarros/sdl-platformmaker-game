#include "daily.hpp"

#include <cstdint>
#include <random>

namespace pm {
namespace {

std::uint32_t hashDate(const std::string& key) {
    std::uint32_t h = 2166136261u;
    for (char c : key) {
        h ^= static_cast<std::uint8_t>(c);
        h *= 16777619u;
    }
    return h;
}

} // namespace

Level makeDailyLevel(const Content& content, const std::string& dateKey) {
    std::mt19937 rng(hashDate(dateKey));

    Level level;
    level.id = "daily/" + dateKey;
    level.title = "Daily " + dateKey;
    level.theme = "grass";
    level.w = 80;
    level.h = 15;
    level.tiles.assign(static_cast<std::size_t>(level.w) * level.h, 0);

    const int ground = content.tileId('#');
    const int platform = content.tileId('=');
    const int brick = content.tileId('B');
    const int question = content.tileId('?');
    const int spike = content.tileId('^');

    auto set = [&](int x, int y, int id) {
        if (x < 0 || y < 0 || x >= level.w || y >= level.h) return;
        level.tiles[static_cast<std::size_t>(y * level.w + x)] = id;
    };

    for (int x = 0; x < level.w; ++x) {
        set(x, 13, ground);
        set(x, 14, ground);
    }

    // pits: a few gaps that get wider as the level goes on
    int x = 12;
    std::uniform_int_distribution<int> gapRoll(0, 9);
    std::uniform_int_distribution<int> gapWidth(2, 3);
    while (x < level.w - 10) {
        x += 6 + static_cast<int>(rng() % 6);
        if (gapRoll(rng) < 6) {
            const int w = gapWidth(rng);
            for (int i = 0; i < w; ++i) {
                set(x + i, 13, 0);
                set(x + i, 14, 0);
            }
            x += w;
        }
    }

    // floating platforms with coins on top
    std::uniform_int_distribution<int> heightRoll(8, 11);
    for (int px = 8; px < level.w - 8; px += 9 + static_cast<int>(rng() % 7)) {
        const int py = heightRoll(rng);
        const int len = 3 + static_cast<int>(rng() % 3);
        for (int i = 0; i < len; ++i) set(px + i, py, platform);
        Entity coin;
        coin.type = "coin";
        coin.tx = px + len / 2;
        coin.ty = py - 1;
        coin.x = float(coin.tx) * kTile;
        coin.y = float(coin.ty) * kTile;
        level.entities.push_back(coin);
    }

    // brick clusters and one question block
    for (int px = 16; px < level.w - 12; px += 14 + static_cast<int>(rng() % 6)) {
        const int py = 9 + static_cast<int>(rng() % 2);
        for (int i = 0; i < 3; ++i) set(px + i, py, brick);
        set(px + 4, py, question);
    }

    // a couple of spike patches on the ground
    for (int i = 0; i < 2; ++i) {
        const int sx = 20 + static_cast<int>(rng() % (level.w - 30));
        for (int k = 0; k < 2; ++k) set(sx + k, 12, spike);
    }

    // walkers
    int walkers = 0;
    for (int px = 18; px < level.w - 10 && walkers < 5; px += 12 + static_cast<int>(rng() % 8)) {
        Entity e;
        e.type = "enemy.walker";
        e.tx = px;
        e.ty = 12;
        e.x = float(px) * kTile;
        e.y = 12.0f * kTile;
        level.entities.push_back(e);
        ++walkers;
    }

    // checkpoint in the middle, goal at the end
    Entity checkpoint;
    checkpoint.type = "checkpoint";
    checkpoint.tx = level.w / 2;
    checkpoint.ty = 11;
    checkpoint.x = float(checkpoint.tx) * kTile;
    checkpoint.y = 11.0f * kTile;
    level.entities.push_back(checkpoint);

    Entity goal;
    goal.type = "goal";
    goal.tx = level.w - 4;
    goal.ty = 10;
    goal.x = float(goal.tx) * kTile;
    goal.y = 10.0f * kTile;
    level.entities.push_back(goal);

    level.startTx = 2;
    level.startTy = 12;
    level.dirty = false;
    return level;
}

} // namespace pm
