#include "level.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace pm {
namespace {

int hexPair(const std::string& s, std::size_t at) {
    return std::stoi(s.substr(at, 2), nullptr, 16);
}

} // namespace

Color parseHex(const std::string& text) {
    Color c;
    if (text.size() < 7 || text[0] != '#') return c;
    c.r = static_cast<Uint8>(hexPair(text, 1));
    c.g = static_cast<Uint8>(hexPair(text, 3));
    c.b = static_cast<Uint8>(hexPair(text, 5));
    c.a = 255;
    return c;
}

int Content::tileId(char symbol) const {
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        if (tiles[i].symbol == symbol) return static_cast<int>(i);
    }
    return -1;
}

const TileDef& Content::tile(int id) const {
    static const TileDef kAir{};
    if (id < 0 || id >= static_cast<int>(tiles.size())) return kAir;
    return tiles[static_cast<std::size_t>(id)];
}

const EntityDef* Content::entity(const std::string& type) const {
    auto it = entities.find(type);
    return it == entities.end() ? nullptr : &it->second;
}

Content Content::load(const std::string& dir) {
    Content content;

    // tiles.json: array, order defines the tile id. First entry should be air.
    std::ifstream tileFile(dir + "tiles.json");
    if (!tileFile) throw std::runtime_error("cannot open " + dir + "tiles.json");
    nlohmann::json tj;
    tileFile >> tj;

    for (const auto& t : tj.at("tiles")) {
        TileDef def;
        const std::string sym = t.at("symbol").get<std::string>();
        def.symbol = sym.empty() ? '.' : sym[0];
        def.name = t.value("name", "tile");
        def.solid = t.value("solid", false);
        def.oneWay = t.value("oneWay", false);
        def.hazard = t.value("hazard", false);
        def.color = parseHex(t.value("color", "#3a3a3a"));
        content.tiles.push_back(def);
    }
    if (content.tiles.empty()) throw std::runtime_error("tiles.json has no tiles");

    // entities.json: array of {type, name, size:[w,h], color}
    std::ifstream entFile(dir + "entities.json");
    if (!entFile) throw std::runtime_error("cannot open " + dir + "entities.json");
    nlohmann::json ej;
    entFile >> ej;

    for (const auto& e : ej.at("entities")) {
        EntityDef def;
        def.type = e.at("type").get<std::string>();
        def.name = e.value("name", def.type);
        const auto size = e.value("size", std::vector<int>{1, 1});
        if (size.size() >= 2) {
            def.w = size[0];
            def.h = size[1];
        }
        def.color = parseHex(e.value("color", "#cccccc"));
        content.entities.emplace(def.type, def);
        content.entityOrder.push_back(def.type);
    }

    return content;
}

int Level::at(int x, int y) const {
    if (!inBounds(x, y)) return 0;
    const auto i = static_cast<std::size_t>(y * w + x);
    if (i >= tiles.size()) return 0;
    return tiles[i];
}

void Level::set(int x, int y, int id) {
    if (!inBounds(x, y)) return;
    tiles[static_cast<std::size_t>(y * w + x)] = id;
    dirty = true;
}

void Level::resize(int newW, int newH) {
    newW = std::max(8, newW);
    newH = std::max(8, newH);

    std::vector<int> next(static_cast<std::size_t>(newW) * newH, 0);
    for (int y = 0; y < std::min(h, newH); ++y) {
        for (int x = 0; x < std::min(w, newW); ++x) {
            next[static_cast<std::size_t>(y * newW + x)] = at(x, y);
        }
    }
    tiles = std::move(next);
    w = newW;
    h = newH;
    dirty = true;
}

Level defaultLevel(const Content& content) {
    Level level;
    level.w = 64;
    level.h = 15;
    level.tiles.assign(static_cast<std::size_t>(level.w) * level.h, 0);

    const int ground = content.tileId('#');
    const int platform = content.tileId('=');

    for (int x = 0; x < level.w; ++x) {
        level.tiles[static_cast<std::size_t>(13 * level.w + x)] = ground;
        level.tiles[static_cast<std::size_t>(14 * level.w + x)] = ground;
    }
    for (int x = 8; x <= 12; ++x) level.tiles[static_cast<std::size_t>(10 * level.w + x)] = platform;
    for (int x = 18; x <= 22; ++x) level.tiles[static_cast<std::size_t>(8 * level.w + x)] = platform;

    level.startTx = 2;
    level.startTy = 12;
    return level;
}

Level loadLevel(const std::string& path, const Content& content) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);

    nlohmann::json j;
    in >> j;

    Level level;
    level.version = j.value("format", 1);
    level.id = j.value("id", "user/untitled");
    level.title = j.value("title", "Untitled");
    level.theme = j.value("theme", "grass");

    const auto rows = j.at("rows").get<std::vector<std::string>>();
    level.h = static_cast<int>(rows.size());
    level.w = rows.empty() ? 16 : static_cast<int>(rows.front().size());
    level.tiles.assign(static_cast<std::size_t>(level.w) * level.h, 0);

    for (int y = 0; y < level.h; ++y) {
        const std::string& row = rows[static_cast<std::size_t>(y)];
        for (int x = 0; x < level.w && x < static_cast<int>(row.size()); ++x) {
            const int id = content.tileId(row[static_cast<std::size_t>(x)]);
            level.tiles[static_cast<std::size_t>(y * level.w + x)] = std::max(0, id);
        }
    }

    if (j.contains("start")) {
        level.startTx = j["start"].value("x", 2);
        level.startTy = j["start"].value("y", level.h - 3);
    }

    if (j.contains("entities")) {
        for (const auto& e : j["entities"]) {
            Entity ent;
            ent.type = e.at("type").get<std::string>();
            ent.tx = e.value("x", 0);
            ent.ty = e.value("y", 0);
            ent.x = float(ent.tx) * kTile;
            ent.y = float(ent.ty) * kTile;
            level.entities.push_back(ent);
        }
    }

    level.dirty = false;
    return level;
}

bool saveLevel(const std::string& path, const Level& level, const Content& content) {
    nlohmann::json j;
    j["format"] = level.version;
    j["id"] = level.id;
    j["title"] = level.title;
    j["theme"] = level.theme;
    j["size"] = {{"w", level.w}, {"h", level.h}};
    j["start"] = {{"x", level.startTx}, {"y", level.startTy}};
    j["legend"] = "tiles.json defines the symbols; entities.json defines entity types";

    std::vector<std::string> rows;
    rows.reserve(static_cast<std::size_t>(level.h));
    for (int y = 0; y < level.h; ++y) {
        std::string row;
        row.reserve(static_cast<std::size_t>(level.w));
        for (int x = 0; x < level.w; ++x) {
            row.push_back(content.tile(level.at(x, y)).symbol);
        }
        rows.push_back(row);
    }
    j["rows"] = rows;

    nlohmann::json ents = nlohmann::json::array();
    for (const auto& e : level.entities) {
        ents.push_back({{"type", e.type}, {"x", e.tx}, {"y", e.ty}});
    }
    j["entities"] = ents;

    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << j.dump(2) << "\n";
    return true;
}

} // namespace pm
