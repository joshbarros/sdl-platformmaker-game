#pragma once

#include <cstdint>
#include <string>
#include <cstdio>
#include <vector>

namespace pm {

// Minimal GIF89a writer with a fixed 3-3-2 palette (256 colours). Enough for
// gameplay clips, zero dependencies, and deterministic output.
class GifWriter {
public:
    bool begin(const std::string& path, int width, int height, int delayMs);
    void addFrame(const std::uint8_t* rgba); // width*height*4, top-down
    bool end();

private:
    void writeByte(std::uint8_t v);
    void writeShort(std::uint16_t v);
    void writeLzw(const std::vector<std::uint8_t>& indices);

    std::FILE* file_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int delay_ = 8;
    bool started_ = false;
};

} // namespace pm
