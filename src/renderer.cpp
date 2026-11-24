#include "renderer.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

namespace pm {
namespace {

bool readFile(const std::string& path, std::vector<unsigned char>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    out.resize(size);
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
    return in.good() || in.eof();
}

} // namespace

Texture Renderer::loadTexture(const std::string& path) {
    Texture texture;
    std::vector<unsigned char> bytes;
    if (!readFile(path, bytes)) return texture;

    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                                  &w, &h, &channels, 4);
    if (!pixels) return texture;

    SDL_Surface* surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, pixels, w * 4);
    if (surface) {
        texture.handle = SDL_CreateTextureFromSurface(renderer_, surface);
        SDL_DestroySurface(surface);
    }

    // Trim transparent padding so sprites line up with their collision box.
    int minX = w, minY = h, maxX = -1, maxY = -1;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (pixels[(static_cast<std::size_t>(y) * w + x) * 4 + 3] > 8) {
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }
        }
    }
    texture.src = (maxX >= minX && maxY >= minY)
                      ? SDL_FRect{float(minX), float(minY), float(maxX - minX + 1),
                                  float(maxY - minY + 1)}
                      : SDL_FRect{0.0f, 0.0f, float(w), float(h)};

    stbi_image_free(pixels);

    if (texture.handle) {
        SDL_SetTextureScaleMode(texture.handle, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(texture.handle, SDL_BLENDMODE_BLEND);
        texture.w = w;
        texture.h = h;
    }
    return texture;
}

void Renderer::drawTextureRotated(const Texture& texture, float x, float y, float w, float h,
                                  bool flipH, double angleDegrees) {
    if (!texture.handle) return;
    const SDL_FRect dst{x, y, w, h};
    const SDL_FPoint center{w * 0.5f, h * 0.5f};
    SDL_RenderTextureRotated(renderer_, texture.handle, &texture.src, &dst, angleDegrees, &center,
                             flipH ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void Renderer::destroyTexture(Texture& texture) {
    if (texture.handle) SDL_DestroyTexture(texture.handle);
    texture = Texture{};
}

void Renderer::drawTexture(const Texture& texture, const SDL_FRect& src, const SDL_FRect& dst,
                           bool flipH) {
    if (!texture.handle) return;
    SDL_RenderTextureRotated(renderer_, texture.handle, &src, &dst, 0.0, nullptr,
                             flipH ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void Renderer::drawTexture(const Texture& texture, float x, float y, float w, float h,
                           bool flipH) {
    if (!texture.handle) return;
    const SDL_FRect src{0.0f, 0.0f, float(texture.w), float(texture.h)};
    const SDL_FRect dst{x, y, w, h};
    drawTexture(texture, src, dst, flipH);
}


bool Renderer::init(const std::string& fontPath, int width, int height) {
    fontPath_ = fontPath;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return false;
    }
    if (!TTF_Init()) {
        return false;
    }

    window_ = SDL_CreateWindow("PLATFORMMAKER", width, height, SDL_WINDOW_RESIZABLE);
    if (!window_) return false;

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) return false;

    SDL_SetRenderVSync(renderer_, 1);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    engine_ = TTF_CreateRendererTextEngine(renderer_);
    if (!engine_) return false;

    return true;
}

void Renderer::shutdown() {
    for (auto& [key, text] : texts_) {
        (void)key;
        TTF_DestroyText(text);
    }
    texts_.clear();

    for (auto& [size, f] : fonts_) {
        (void)size;
        TTF_CloseFont(f);
    }
    fonts_.clear();

    if (engine_) TTF_DestroyRendererTextEngine(engine_);
    engine_ = nullptr;

    if (renderer_) SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
    if (window_) SDL_DestroyWindow(window_);
    window_ = nullptr;

    TTF_Quit();
    SDL_Quit();
}

bool Renderer::worldSpace() {
    int w = 0, h = 0;
    SDL_GetWindowSize(window_, &w, &h);
    const float scale = std::max(1.0f, std::min(float(w) / 640.0f, float(h) / 360.0f));
    // SDL multiplies the viewport rectangle by the render scale, so the
    // viewport is given in virtual units and the letterbox offset is divided
    // by the scale.
    const float padX = (float(w) - 640.0f * scale) * 0.5f / scale;
    const float padY = (float(h) - 360.0f * scale) * 0.5f / scale;
    SDL_Rect viewport{int(padX), int(padY), 640, 360};
    SDL_SetRenderViewport(renderer_, &viewport);
    return SDL_SetRenderScale(renderer_, scale, scale);
}

void Renderer::screenSpace() {
    SDL_SetRenderViewport(renderer_, nullptr);
    SDL_SetRenderScale(renderer_, 1.0f, 1.0f);
}


void Renderer::beginFrame(Color clear) {
    SDL_SetRenderDrawColor(renderer_, clear.r, clear.g, clear.b, clear.a);
    SDL_RenderClear(renderer_);
}

void Renderer::endFrame() {
    SDL_RenderPresent(renderer_);
}

void Renderer::fillRect(float x, float y, float w, float h, Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    const SDL_FRect r{x, y, w, h};
    SDL_RenderFillRect(renderer_, &r);
}

void Renderer::frameRect(float x, float y, float w, float h, Color c, float thickness) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    const SDL_FRect top{x, y, w, thickness};
    const SDL_FRect bottom{x, y + h - thickness, w, thickness};
    const SDL_FRect left{x, y + thickness, thickness, h - thickness * 2};
    const SDL_FRect right{x + w - thickness, y + thickness, thickness, h - thickness * 2};
    SDL_RenderFillRect(renderer_, &top);
    SDL_RenderFillRect(renderer_, &bottom);
    SDL_RenderFillRect(renderer_, &left);
    SDL_RenderFillRect(renderer_, &right);
}

void Renderer::filledCircle(float cx, float cy, float radius, Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    const int r = static_cast<int>(radius);
    for (int dy = -r; dy <= r; ++dy) {
        const float half = std::sqrt(std::max(0.0f, radius * radius - float(dy * dy)));
        SDL_RenderLine(renderer_, cx - half, cy + float(dy), cx + half, cy + float(dy));
    }
}

void Renderer::line(float x1, float y1, float x2, float y2, Color c, float thickness) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    if (thickness <= 1.0f) {
        SDL_RenderLine(renderer_, x1, y1, x2, y2);
        return;
    }
    // thick line as a rotated rectangle
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.001f) return;
    const float nx = -dy / len * (thickness * 0.5f);
    const float ny = dx / len * (thickness * 0.5f);
    const SDL_FColor fc{float(c.r) / 255.0f, float(c.g) / 255.0f, float(c.b) / 255.0f,
                        float(c.a) / 255.0f};
    SDL_Vertex verts[4] = {
        {{x1 + nx, y1 + ny}, fc, {0, 0}},
        {{x2 + nx, y2 + ny}, fc, {0, 0}},
        {{x2 - nx, y2 - ny}, fc, {0, 0}},
        {{x1 - nx, y1 - ny}, fc, {0, 0}},
    };
    const int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(renderer_, nullptr, verts, 4, indices, 6);
}

TTF_Font* Renderer::font(int size) {
    auto it = fonts_.find(size);
    if (it != fonts_.end()) return it->second;

    TTF_Font* f = TTF_OpenFont(fontPath_.c_str(), float(size));
    if (!f) {
        throw std::runtime_error(std::string("cannot open font: ") + SDL_GetError());
    }
    fonts_.emplace(size, f);
    return f;
}

TTF_Text* Renderer::cachedText(const std::string& value, int size, Color c) {
    const std::string key = std::to_string(size) + "|" + std::to_string(c.r) + "," +
                            std::to_string(c.g) + "," + std::to_string(c.b) + "," +
                            std::to_string(c.a) + "|" + value;
    auto it = texts_.find(key);
    if (it != texts_.end()) return it->second;

    TTF_Text* t = TTF_CreateText(engine_, font(size), value.c_str(), 0);
    if (t) TTF_SetTextColor(t, c.r, c.g, c.b, c.a);
    texts_.emplace(key, t);
    return t;
}

void Renderer::text(const std::string& value, float x, float y, int size, Color c, Align align) {
    TTF_Text* t = cachedText(value, size, c);
    if (!t) return;

    int w = 0, h = 0;
    TTF_GetTextSize(t, &w, &h);
    float drawX = x;
    if (align == Align::Center) drawX = x - float(w) * 0.5f;
    if (align == Align::Right) drawX = x - float(w);
    TTF_DrawRendererText(t, drawX, y);
}

float Renderer::textWidth(const std::string& value, int size) {
    TTF_Text* t = cachedText(value, size, palette::white);
    if (!t) return 0.0f;
    int w = 0, h = 0;
    TTF_GetTextSize(t, &w, &h);
    return float(w);
}

float Renderer::textHeight(int size) {
    TTF_Text* t = cachedText("Ag", size, palette::white);
    if (!t) return float(size);
    int w = 0, h = 0;
    TTF_GetTextSize(t, &w, &h);
    return float(h);
}

bool Renderer::screenshot(const std::string& path) {
    SDL_Surface* surface = SDL_RenderReadPixels(renderer_, nullptr);
    if (!surface) return false;
    const bool ok = SDL_SaveBMP(surface, path.c_str());
    SDL_DestroySurface(surface);
    return ok;
}

} // namespace pm
