#pragma once

#include <map>
#include <string>
#include <vector>

#include "renderer.hpp"

namespace pm {

constexpr int kTile = 32;

struct TileDef {
    char symbol = '.';
    std::string name = "air";
    bool solid = false;
    bool oneWay = false;
    bool hazard = false;
    Color color{0x2a, 0x2a, 0x2a, 255};
};

struct EntityDef {
    std::string type;
    std::string name;
    int w = 1;
    int h = 1;
    Color color{};
};

// Tile symbols and entity types come from JSON so packs can extend them later
// without touching the level format or the code.
struct Content {
    std::vector<TileDef> tiles;
    std::map<std::string, EntityDef> entities;
    std::vector<std::string> entityOrder;

    int tileId(char symbol) const;
    const TileDef& tile(int id) const;
    const EntityDef* entity(const std::string& type) const;
    static Content load(const std::string& dir);
};

struct Entity {
    std::string type;
    int tx = 0;
    int ty = 0;
    // runtime state (not serialized)
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    bool alive = true;
    bool touched = false;
};

struct Level {
    int version = 1;
    std::string id = "user/untitled";
    std::string title = "Untitled";
    std::string theme = "grass";
    int w = 64;
    int h = 15;
    std::vector<int> tiles;
    std::vector<Entity> entities;
    int startTx = 2;
    int startTy = 12;
    bool dirty = false;

    Level() { resize(w, h); }

    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    int at(int x, int y) const;
    void set(int x, int y, int id);
    void resize(int newW, int newH);
};

Color parseHex(const std::string& text);

Level defaultLevel(const Content& content);
Level loadLevel(const std::string& path, const Content& content);
bool saveLevel(const std::string& path, const Level& level, const Content& content);

} // namespace pm
