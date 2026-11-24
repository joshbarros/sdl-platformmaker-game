#include "replay.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace pm {

void ReplayRecorder::reset(const std::string& levelCode) {
    replay_ = Replay{};
    replay_.levelCode = levelCode;
}

void ReplayRecorder::step(const InputState& input) {
    const int bit = replay_.steps * 4;
    if (bit / 8 >= static_cast<int>(replay_.bits.size())) replay_.bits.resize(bit / 8 + 1, 0);

    std::uint8_t value = 0;
    if (input.left) value |= 0x1;
    if (input.right) value |= 0x2;
    if (input.jump) value |= 0x4;
    if (input.run) value |= 0x8;

    const int byteIndex = bit / 8;
    const int shift = bit % 8;
    replay_.bits[static_cast<std::size_t>(byteIndex)] |=
        static_cast<std::uint8_t>(value << shift);
    if (shift > 4) {
        // the 4-bit value straddles two bytes
        replay_.bits[static_cast<std::size_t>(byteIndex + 1)] |=
            static_cast<std::uint8_t>(value >> (8 - shift));
    }
    ++replay_.steps;
}

ReplayPlayer::ReplayPlayer(Replay replay) : replay_(std::move(replay)) {}

InputState ReplayPlayer::next() {
    InputState input;
    if (index_ >= replay_.steps) return input;

    const int bit = index_ * 4;
    const int byteIndex = bit / 8;
    const int shift = bit % 8;
    std::uint32_t packed = replay_.bits[static_cast<std::size_t>(byteIndex)];
    if (shift > 4 && byteIndex + 1 < static_cast<int>(replay_.bits.size())) {
        packed |= std::uint32_t(replay_.bits[static_cast<std::size_t>(byteIndex + 1)]) << 8;
    }
    const std::uint8_t value = static_cast<std::uint8_t>((packed >> shift) & 0xF);

    input.left = (value & 0x1) != 0;
    input.right = (value & 0x2) != 0;
    input.jump = (value & 0x4) != 0;
    input.run = (value & 0x8) != 0;
    input.jumpPressed = input.jump && !previousJump_;
    previousJump_ = input.jump;

    ++index_;
    return input;
}

namespace {

std::string base64(const std::vector<std::uint8_t>& data) {
    static const char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
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
        out.push_back('=');
        out.push_back('=');
    } else if (rest == 2) {
        const std::uint32_t v = (std::uint32_t(data[i]) << 16) | (std::uint32_t(data[i + 1]) << 8);
        out.push_back(kAlphabet[(v >> 18) & 0x3F]);
        out.push_back(kAlphabet[(v >> 12) & 0x3F]);
        out.push_back(kAlphabet[(v >> 6) & 0x3F]);
        out.push_back('=');
    }
    return out;
}

bool unbase64(const std::string& text, std::vector<std::uint8_t>& out) {
    int buffer = 0;
    int bits = 0;
    for (char c : text) {
        int value = -1;
        if (c >= 'A' && c <= 'Z') value = c - 'A';
        else if (c >= 'a' && c <= 'z') value = c - 'a' + 26;
        else if (c >= '0' && c <= '9') value = c - '0' + 52;
        else if (c == '+') value = 62;
        else if (c == '/') value = 63;
        else if (c == '=') break;
        else return false;

        buffer = (buffer << 6) | value;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFF));
        }
    }
    return true;
}

} // namespace

std::string encodeReplay(const Replay& replay) {
    nlohmann::json j;
    j["format"] = 1;
    j["level"] = replay.levelCode;
    j["steps"] = replay.steps;
    j["inputs"] = base64(replay.bits);
    return j.dump();
}

bool decodeReplay(const std::string& text, Replay& out) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(text);
    } catch (const std::exception&) {
        return false;
    }
    if (!j.contains("format") || j["format"].get<int>() != 1) return false;

    Replay replay;
    replay.levelCode = j.value("level", "");
    replay.steps = j.value("steps", 0);
    const std::string inputs = j.value("inputs", "");
    if (replay.steps < 0 || !unbase64(inputs, replay.bits)) return false;
    const std::size_t needed = (static_cast<std::size_t>(replay.steps) * 4 + 7) / 8;
    if (replay.bits.size() < needed) return false;

    out = std::move(replay);
    return true;
}

bool saveReplay(const std::string& path, const Replay& replay) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << encodeReplay(replay) << "\n";
    return true;
}

bool loadReplay(const std::string& path, Replay& out) {
    std::ifstream in(path);
    if (!in) return false;
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return decodeReplay(text, out);
}

} // namespace pm
