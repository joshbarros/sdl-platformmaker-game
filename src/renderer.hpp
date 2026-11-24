#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <string>
#include <unordered_map>

namespace pm {

struct Color {
    Uint8 r = 0, g = 0, b = 0, a = 255;
};

namespace palette {
inline constexpr Color bg{0x14, 0x0f, 0x1c};
inline constexpr Color panel{0x24, 0x1a, 0x33};
inline constexpr Color panelDark{0x1b, 0x13, 0x27};
inline constexpr Color parchment{0xf2, 0xe9, 0xd8};
inline constexpr Color ink{0x24, 0x1a, 0x33};
inline constexpr Color accent{0xff, 0xcf, 0x6b};
inline constexpr Color sword{0xe2, 0x60, 0x4a};
inline constexpr Color shield{0x4a, 0xa7, 0xe2};
inline constexpr Color potion{0x57, 0xc6, 0x8a};
inline constexpr Color curse{0xb0, 0x6b, 0xd6};
inline constexpr Color monsterBody{0x3a, 0x16, 0x20};
inline constexpr Color monsterEdge{0x8e, 0x2d, 0x3d};
inline constexpr Color dim{0x6b, 0x5f, 0x7a};
inline constexpr Color white{0xff, 0xff, 0xff};
} // namespace palette

enum class Align { Left, Center, Right };

// Loaded image + its pixel size. Textures are created with nearest-neighbour
// scaling so pixel art stays crisp when the sprite is upscaled.
struct Texture {
    SDL_Texture* handle = nullptr;
    int w = 0;
    int h = 0;
    // Opaque bounding box inside the image, so sprites with transparent padding
    // line up with the collision box instead of floating at the wrong scale.
    SDL_FRect src{0.0f, 0.0f, 0.0f, 0.0f};

    bool valid() const { return handle != nullptr; }
};

// Thin immediate-mode 2D layer on top of SDL_Renderer. No UI framework, no
// retained widgets: the game draws its whole screen every frame.
class Renderer {
public:
    bool init(const std::string& fontPath, int width, int height);
    void shutdown();

    Texture loadTexture(const std::string& path);
    void destroyTexture(Texture& texture);
    void drawTexture(const Texture& texture, const SDL_FRect& src, const SDL_FRect& dst,
                     bool flipH = false);
    void drawTexture(const Texture& texture, float x, float y, float w, float h,
                     bool flipH = false);

    SDL_Window* window() const { return window_; }
    SDL_Renderer* sdl() const { return renderer_; }

    void beginFrame(Color clear);
    void endFrame();

    void fillRect(float x, float y, float w, float h, Color c);
    void frameRect(float x, float y, float w, float h, Color c, float thickness = 2.0f);
    void filledCircle(float cx, float cy, float radius, Color c);
    void line(float x1, float y1, float x2, float y2, Color c, float thickness = 2.0f);

    void text(const std::string& value, float x, float y, int size, Color c,
              Align align = Align::Left);
    float textWidth(const std::string& value, int size);
    float textHeight(int size);

    bool screenshot(const std::string& path);

private:
    TTF_Font* font(int size);
    TTF_Text* cachedText(const std::string& value, int size, Color c);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    TTF_TextEngine* engine_ = nullptr;
    std::string fontPath_;
    std::unordered_map<int, TTF_Font*> fonts_;
    std::unordered_map<std::string, TTF_Text*> texts_;
};

} // namespace pm
