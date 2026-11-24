#pragma once

#include "level.hpp"

namespace pm {

struct InputState {
    bool left = false;
    bool right = false;
    bool jump = false;
    bool jumpPressed = false; // rising edge this frame
    bool run = false;
};

struct Player {
    float x = 0.0f;
    float y = 0.0f;
    float w = 22.0f;
    float h = 44.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    bool onGround = false;
    bool facingRight = true;
    float coyote = 0.0f;
    float buffer = 0.0f;
    float respawnX = 0.0f;
    float respawnY = 0.0f;
};

struct Particle {
    float x = 0.0f, y = 0.0f, vx = 0.0f, vy = 0.0f;
    float life = 0.0f, maxLife = 1.0f;
    float size = 2.0f;
    Color color{255, 255, 255, 255};
};

struct Camera {
    float x = 0.0f;
    float y = 0.0f;
};

// Runtime simulation for one level. Owns no rendering; the level is plain data
// so an editor and the game can share exactly the same struct.
class World {
public:
    World(Level level, const Content& content);

    void setViewport(int width, int height);
    void resetCamera();
    void restart();
    void update(float dt, const InputState& input);

    const Level& level() const { return level_; }
    const Player& player() const { return player_; }
    Camera camera() const { return camera_; }
    const std::vector<Particle>& particles() const { return particles_; }
    int coins() const { return coins_; }
    int deaths() const { return deaths_; }
    bool finished() const { return finished_; }
    float elapsed() const { return elapsed_; }
    std::vector<Entity>& entities() { return level_.entities; }
    const std::vector<Entity>& entities() const { return level_.entities; }

private:
    bool solidAt(int tx, int ty) const;
    bool oneWayAt(int tx, int ty) const;
    bool hazardAt(int tx, int ty) const;
    bool rectHitsSolid(float x, float y, float w, float h) const;
    void movePlayerX(float dx);
    void movePlayerY(float dy);
    void hurt();
    void updateEntities(float dt);
    void resolveEntityY(Entity& e, float dy);
    void checkEntityTouches();
    void updateCamera();

    Level level_;
    const Content* content_ = nullptr;
    Player player_;
    Camera camera_;
    int viewW_ = 1280;
    int viewH_ = 800;
    int coins_ = 0;
    int deaths_ = 0;
    bool finished_ = false;
    float elapsed_ = 0.0f;
    std::vector<Particle> particles_;
    float shake_ = 0.0f;
    bool wasOnGround_ = false;
    void spawnParticles(float x, float y, int count, Color color, float spread);
};

} // namespace pm
