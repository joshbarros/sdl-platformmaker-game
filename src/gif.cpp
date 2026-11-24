#include "gif.hpp"

#include <algorithm>
#include <cstring>

namespace pm {
namespace {

constexpr int kCodeSize = 8;
constexpr int kClearCode = 1 << kCodeSize;      // 256
constexpr int kEndCode = kClearCode + 1;        // 257

struct BitWriter {
    std::vector<std::uint8_t> bytes;
    std::uint32_t acc = 0;
    int bits = 0;

    void write(int code, int codeLength) {
        acc |= std::uint32_t(code) << bits;
        bits += codeLength;
        while (bits >= 8) {
            bytes.push_back(std::uint8_t(acc & 0xFF));
            acc >>= 8;
            bits -= 8;
        }
    }

    void flush() {
        while (bits > 0) {
            bytes.push_back(std::uint8_t(acc & 0xFF));
            acc >>= 8;
            bits -= 8;
        }
    }
};

} // namespace

bool GifWriter::begin(const std::string& path, int width, int height, int delayMs) {
    file_ = std::fopen(path.c_str(), "wb");
    if (!file_) return false;
    width_ = width;
    height_ = height;
    delay_ = delayMs / 10;

    std::fwrite("GIF89a", 1, 6, file_);
    writeShort(std::uint16_t(width_));
    writeShort(std::uint16_t(height_));
    writeByte(0xF7); // global table, 256 entries, 8 bits
    writeByte(0);    // background
    writeByte(0);    // aspect

    for (int i = 0; i < 256; ++i) {
        const std::uint8_t r = std::uint8_t(((i >> 5) & 0x7) * 255 / 7);
        const std::uint8_t g = std::uint8_t(((i >> 2) & 0x7) * 255 / 7);
        const std::uint8_t b = std::uint8_t((i & 0x3) * 255 / 3);
        writeByte(r);
        writeByte(g);
        writeByte(b);
    }

    // loop forever
    writeByte(0x21);
    writeByte(0xFF);
    writeByte(0x0B);
    std::fwrite("NETSCAPE2.0", 1, 11, file_);
    writeByte(0x03);
    writeByte(0x01);
    writeShort(0);
    writeByte(0x00);
    return true;
}

void GifWriter::writeByte(std::uint8_t v) { std::fputc(v, file_); }

void GifWriter::writeShort(std::uint16_t v) {
    writeByte(std::uint8_t(v & 0xFF));
    writeByte(std::uint8_t((v >> 8) & 0xFF));
}

void GifWriter::addFrame(const std::uint8_t* rgba) {
    if (!file_) return;

    std::vector<std::uint8_t> indices(std::size_t(width_) * height_);
    for (int i = 0; i < width_ * height_; ++i) {
        const std::uint8_t r = rgba[i * 4 + 0];
        const std::uint8_t g = rgba[i * 4 + 1];
        const std::uint8_t b = rgba[i * 4 + 2];
        indices[std::size_t(i)] =
            std::uint8_t(((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6));
    }

    writeByte(0x21); // graphic control
    writeByte(0xF9);
    writeByte(0x04);
    writeByte(0x04); // disposal: restore to background
    writeShort(std::uint16_t(delay_));
    writeByte(0x00);
    writeByte(0x00);

    writeByte(0x2C); // image descriptor
    writeShort(0);
    writeShort(0);
    writeShort(std::uint16_t(width_));
    writeShort(std::uint16_t(height_));
    writeByte(0x00);

    writeByte(kCodeSize);
    writeLzw(indices);
}

void GifWriter::writeLzw(const std::vector<std::uint8_t>& indices) {
    // Classic LZW with a 4096-entry dictionary rebuilt from scratch per frame.
    std::vector<int> dictionary(4096 * 256, -1);
    auto reset = [&]() { std::fill(dictionary.begin(), dictionary.end(), -1); };

    reset();
    BitWriter out;
    int codeLength = kCodeSize + 1;
    int nextCode = kEndCode + 1;
    out.write(kClearCode, codeLength);

    int prefix = indices.empty() ? 0 : indices[0];
    for (std::size_t i = 1; i < indices.size(); ++i) {
        const int suffix = indices[i];
        int& slot = dictionary[std::size_t(prefix) * 256 + suffix];
        if (slot >= 0) {
            prefix = slot;
            continue;
        }
        out.write(prefix, codeLength);
        if (nextCode < 4096) {
            slot = nextCode++;
            if (nextCode > (1 << codeLength) && codeLength < 12) ++codeLength;
        } else {
            out.write(kClearCode, codeLength);
            reset();
            codeLength = kCodeSize + 1;
            nextCode = kEndCode + 1;
        }
        prefix = suffix;
    }
    out.write(prefix, codeLength);
    out.write(kEndCode, codeLength);
    out.flush();

    // sub-blocks of at most 255 bytes
    std::size_t at = 0;
    while (at < out.bytes.size()) {
        const std::size_t chunk = std::min<std::size_t>(255, out.bytes.size() - at);
        writeByte(std::uint8_t(chunk));
        std::fwrite(out.bytes.data() + at, 1, chunk, file_);
        at += chunk;
    }
    writeByte(0x00);
}

bool GifWriter::end() {
    if (!file_) return false;
    writeByte(0x3B);
    std::fclose(file_);
    file_ = nullptr;
    return true;
}

} // namespace pm
