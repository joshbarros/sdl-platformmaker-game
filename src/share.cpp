#include "share.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace pm {
namespace {

constexpr const char* kPrefix = "PM1-";
constexpr char kAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

const char* themeNames[] = {"grass", "cave", "night"};

std::uint8_t themeId(const std::string& name) {
    for (std::uint8_t i = 0; i < 3; ++i) {
        if (name == themeNames[i]) return i;
    }
    return 0;
}

std::string base64UrlEncode(const std::vector<std::uint8_t>& data) {
    std::string out;
    out.reserve((data.size() * 4 + 2) / 3 + 1);

    std::size_t i = 0;
    while (i + 2 < data.size()) {
        const std::uint32_t v =
            (std::uint32_t(data[i]) << 16) | (std::uint32_t(data[i + 1]) << 8) | data[i + 2];
        out.push_back(kAlphabet[(v >> 18) & 0x3F]);
        out.push_back(kAlphabet[(v >> 12) & 0x3F]);
        out.push_back(kAlphabet[(v >> 6) & 0x3F]);
        out.push_back(kAlphabet[v & 0x3F]);
        i += 3;
    }
    const std::size_t rest = data.size() - i;
    if (rest == 1) {
        const std::uint32_t v = std::uint32_t(data[i]) << 16;
        out.push_back(kAlphabet[(v >> 18) & 0x3F]);
        out.push_back(kAlphabet[(v >> 12) & 0x3F]);
    } else if (rest == 2) {
        const std::uint32_t v = (std::uint32_t(data[i]) << 16) | (std::uint32_t(data[i + 1]) << 8);
        out.push_back(kAlphabet[(v >> 18) & 0x3F]);
        out.push_back(kAlphabet[(v >> 12) & 0x3F]);
        out.push_back(kAlphabet[(v >> 6) & 0x3F]);
    }
    return out;
}

int base64Value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
}

bool base64UrlDecode(const std::string& text, std::vector<std::uint8_t>& out) {
    std::uint32_t buffer = 0;
    int bits = 0;
    for (char c : text) {
        const int value = base64Value(c);
        if (value < 0) return false;
        buffer = (buffer << 6) | std::uint32_t(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFF));
        }
    }
    return true;
}

void putU16(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

std::uint32_t getU16(const std::vector<std::uint8_t>& in, std::size_t& at) {
    const std::uint32_t lo = in[at];
    const std::uint32_t hi = in[at + 1];
    at += 2;
    return lo | (hi << 8);
}

} // namespace

std::string encodeLevelCode(const Level& level, const Content& content) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(256);

    bytes.push_back('P');
    bytes.push_back('M');
    bytes.push_back(1);
    putU16(bytes, static_cast<std::uint32_t>(level.w));
    putU16(bytes, static_cast<std::uint32_t>(level.h));
    putU16(bytes, static_cast<std::uint32_t>(level.startTx));
    putU16(bytes, static_cast<std::uint32_t>(level.startTy));
    bytes.push_back(themeId(level.theme));

    // tiles: run-length encoded (id, chunks of up to 255)
    std::size_t i = 0;
    const std::size_t total = static_cast<std::size_t>(level.w) * level.h;
    while (i < total) {
        const std::uint8_t id = static_cast<std::uint8_t>(
            std::clamp(level.tiles[i], 0, 255));
        std::size_t run = 1;
        while (i + run < total && run < 65535 &&
               std::clamp(level.tiles[i + run], 0, 255) == id) {
            ++run;
        }
        bytes.push_back(id);
        std::size_t remaining = run;
        while (remaining >= 255) {
            bytes.push_back(255);
            remaining -= 255;
        }
        bytes.push_back(static_cast<std::uint8_t>(remaining));
        i += run;
    }

    // entities: type id + grid position
    std::vector<Entity> known;
    for (const auto& e : level.entities) {
        const auto it = std::find(content.entityOrder.begin(), content.entityOrder.end(), e.type);
        if (it == content.entityOrder.end()) continue;
        known.push_back(e);
    }
    putU16(bytes, static_cast<std::uint32_t>(known.size()));
    for (const auto& e : known) {
        const auto it = std::find(content.entityOrder.begin(), content.entityOrder.end(), e.type);
        const auto index = static_cast<std::uint8_t>(it - content.entityOrder.begin());
        bytes.push_back(index);
        putU16(bytes, static_cast<std::uint32_t>(e.tx));
        putU16(bytes, static_cast<std::uint32_t>(e.ty));
    }

    return std::string(kPrefix) + base64UrlEncode(bytes);
}

std::string decodeLevelCode(const std::string& code, const Content& content, Level& out) {
    std::string body = code;
    // tolerate surrounding whitespace and a pasted prefix in any case
    while (!body.empty() && (body.back() == '\n' || body.back() == '\r' || body.back() == ' ')) {
        body.pop_back();
    }
    const std::string prefix(kPrefix);
    if (body.size() < prefix.size() ||
        body.compare(0, prefix.size(), prefix) != 0) {
        return "code must start with PM1-";
    }
    body = body.substr(prefix.size());

    std::vector<std::uint8_t> bytes;
    if (!base64UrlDecode(body, bytes)) return "code has invalid characters";
    if (bytes.size() < 12) return "code is truncated";
    if (bytes[0] != 'P' || bytes[1] != 'M') return "code has a bad header";
    if (bytes[2] != 1) return "unsupported code version";

    std::size_t at = 3;
    const std::uint32_t w = getU16(bytes, at);
    const std::uint32_t h = getU16(bytes, at);
    const std::uint32_t startTx = getU16(bytes, at);
    const std::uint32_t startTy = getU16(bytes, at);
    if (w < 8 || h < 8 || w > 1024 || h > 512) return "code has an invalid size";

    Level level;
    level.w = static_cast<int>(w);
    level.h = static_cast<int>(h);
    level.tiles.assign(static_cast<std::size_t>(w) * h, 0);
    level.startTx = static_cast<int>(startTx);
    level.startTy = static_cast<int>(startTy);
    if (at >= bytes.size()) return "code is truncated";
    const std::uint8_t theme = bytes[at++];
    level.theme = theme < 3 ? themeNames[theme] : "grass";

    std::size_t filled = 0;
    const std::size_t total = level.tiles.size();
    while (filled < total) {
        if (at + 1 >= bytes.size()) return "code is truncated";
        const std::uint8_t id = bytes[at++];
        std::size_t run = 0;
        bool terminated = false;
        while (at < bytes.size()) {
            const std::uint8_t chunk = bytes[at++];
            if (chunk == 255) {
                run += 255;
                continue;
            }
            run += chunk;
            terminated = true;
            break;
        }
        if (!terminated) return "code is truncated";
        if (run == 0) return "code has a zero-length run";
        if (filled + run > total) return "code has more tiles than its size allows";
        for (std::size_t k = 0; k < run; ++k) level.tiles[filled + k] = id;
        filled += run;
    }

    if (at + 2 > bytes.size()) return "code is truncated";
    const std::uint32_t entityCount = getU16(bytes, at);
    if (entityCount > 4000) return "code has too many entities";
    for (std::uint32_t i = 0; i < entityCount; ++i) {
        if (at + 5 > bytes.size()) return "code is truncated";
        const std::uint8_t typeId = bytes[at++];
        if (typeId >= content.entityOrder.size()) return "code uses an unknown entity type";
        Entity ent;
        ent.type = content.entityOrder[typeId];
        ent.tx = static_cast<int>(getU16(bytes, at));
        ent.ty = static_cast<int>(getU16(bytes, at));
        ent.x = float(ent.tx) * kTile;
        ent.y = float(ent.ty) * kTile;
        level.entities.push_back(ent);
    }

    level.title = "Shared level";
    level.id = "shared/code";
    level.dirty = false;
    out = std::move(level);
    return {};
}

} // namespace pm
