#include "world.hpp"

#include <algorithm>
#include <cmath>

namespace pm {
namespace {

constexpr float kGravity = 1900.0f;
constexpr float kMaxFall = 1100.0f;
constexpr float kWalkSpeed = 230.0f;
constexpr float kRunSpeed = 340.0f;
constexpr float kGroundAccel = 2400.0f;
constexpr float kAirAccel = 1500.0f;
constexpr float kGroundFriction = 2600.0f;
constexpr float kAirFriction = 500.0f;
constexpr float kJumpVelocity = -640.0f;
constexpr float kCoyoteTime = 0.09f;
constexpr float kJumpBuffer = 0.12f;
constexpr float kEnemySpeed = 70.0f;
constexpr float kStompBounce = -430.0f;

bool overlaps(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

} // namespace

World::World(Level level, const Content& content)
    : level_(std::move(level)), content_(&content) {
    restart();
}

void World::setViewport(int width, int height) {
    viewW_ = std::max(320, width);
    viewH_ = std::max(240, height);
}

void World::restart() {
    for (auto& e : level_.entities) {
        e.alive = true;
        e.touched = false;
        e.x = float(e.tx) * kTile;
        e.y = float(e.ty) * kTile;
        e.vx = 0.0f;
        e.vy = 0.0f;
    }

    player_ = Player{};
    player_.x = float(level_.startTx) * kTile + (kTile - player_.w) * 0.5f;
    player_.y = float(level_.startTy) * kTile + (kTile - player_.h);
    player_.respawnX = player_.x;
    player_.respawnY = player_.y;

    coins_ = 0;
    deaths_ = 0;
    finished_ = false;
    elapsed_ = 0.0f;
    camera_ = Camera{};
    updateCamera();
}

bool World::solidAt(int tx, int ty) const {
    if (!level_.inBounds(tx, ty)) return false;
    const TileDef& t = content_->tile(level_.at(tx, ty));
    return t.solid && !t.oneWay;
}

bool World::oneWayAt(int tx, int ty) const {
    if (!level_.inBounds(tx, ty)) return false;
    const TileDef& t = content_->tile(level_.at(tx, ty));
    return t.solid && t.oneWay;
}

bool World::hazardAt(int tx, int ty) const {
    if (!level_.inBounds(tx, ty)) return false;
    return content_->tile(level_.at(tx, ty)).hazard;
}

bool World::rectHitsSolid(float x, float y, float w, float h) const {
    const int left = static_cast<int>(std::floor(x / kTile));
    const int right = static_cast<int>(std::floor((x + w - 1.0f) / kTile));
    const int top = static_cast<int>(std::floor(y / kTile));
    const int bottom = static_cast<int>(std::floor((y + h - 1.0f) / kTile));
    for (int ty = top; ty <= bottom; ++ty) {
        for (int tx = left; tx <= right; ++tx) {
            if (solidAt(tx, ty)) return true;
        }
    }
    return false;
}

void World::update(float dt, const InputState& input) {
    if (finished_) return;
    elapsed_ += dt;

    const float maxSpeed = input.run ? kRunSpeed : kWalkSpeed;
    const float accel = player_.onGround ? kGroundAccel : kAirAccel;
    const float friction = player_.onGround ? kGroundFriction : kAirFriction;

    if (input.right && !input.left) {
        player_.vx += accel * dt;
        player_.facingRight = true;
    } else if (input.left && !input.right) {
        player_.vx -= accel * dt;
        player_.facingRight = false;
    } else {
        if (player_.vx > 0.0f) player_.vx = std::max(0.0f, player_.vx - friction * dt);
        if (player_.vx < 0.0f) player_.vx = std::min(0.0f, player_.vx + friction * dt);
    }
    player_.vx = std::clamp(player_.vx, -maxSpeed, maxSpeed);

    if (input.jumpPressed) player_.buffer = kJumpBuffer;
    player_.buffer = std::max(0.0f, player_.buffer - dt);
    player_.coyote = player_.onGround ? kCoyoteTime : std::max(0.0f, player_.coyote - dt);

    if (player_.buffer > 0.0f && player_.coyote > 0.0f) {
        player_.vy = kJumpVelocity;
        player_.buffer = 0.0f;
        player_.coyote = 0.0f;
        player_.onGround = false;
    }
    // variable jump height: releasing early cuts the ascent
    if (!input.jump && player_.vy < -220.0f) player_.vy = -220.0f;

    player_.vy = std::min(player_.vy + kGravity * dt, kMaxFall);

    player_.onGround = false;
    movePlayerX(player_.vx * dt);
    movePlayerY(player_.vy * dt);

    // hazards and pits
    const int leftTile = static_cast<int>(std::floor(player_.x / kTile));
    const int rightTile = static_cast<int>(std::floor((player_.x + player_.w - 1.0f) / kTile));
    const int topTile = static_cast<int>(std::floor(player_.y / kTile));
    const int bottomTile = static_cast<int>(std::floor((player_.y + player_.h - 1.0f) / kTile));
    for (int ty = topTile; ty <= bottomTile; ++ty) {
        for (int tx = leftTile; tx <= rightTile; ++tx) {
            if (hazardAt(tx, ty)) {
                hurt();
                return;
            }
        }
    }
    if (player_.y > float(level_.h) * kTile + 120.0f) {
        hurt();
        return;
    }

    updateEntities(dt);
    checkEntityTouches();
    updateCamera();
}

void World::movePlayerX(float dx) {
    if (dx == 0.0f) return;
    player_.x += dx;

    const int top = static_cast<int>(std::floor(player_.y / kTile));
    const int bottom = static_cast<int>(std::floor((player_.y + player_.h - 1.0f) / kTile));

    if (dx > 0.0f) {
        const int right = static_cast<int>(std::floor((player_.x + player_.w - 1.0f) / kTile));
        for (int ty = top; ty <= bottom; ++ty) {
            if (solidAt(right, ty)) {
                player_.x = float(right) * kTile - player_.w;
                player_.vx = 0.0f;
                break;
            }
        }
    } else {
        const int left = static_cast<int>(std::floor(player_.x / kTile));
        for (int ty = top; ty <= bottom; ++ty) {
            if (solidAt(left, ty)) {
                player_.x = float(left + 1) * kTile;
                player_.vx = 0.0f;
                break;
            }
        }
    }
}

void World::movePlayerY(float dy) {
    if (dy == 0.0f) return;

    const float previousBottom = player_.y + player_.h;
    player_.y += dy;

    const int left = static_cast<int>(std::floor(player_.x / kTile));
    const int right = static_cast<int>(std::floor((player_.x + player_.w - 1.0f) / kTile));

    if (dy > 0.0f) {
        const int bottom = static_cast<int>(std::floor((player_.y + player_.h - 1.0f) / kTile));
        bool landed = false;
        for (int tx = left; tx <= right; ++tx) {
            const bool blocked = solidAt(tx, bottom) ||
                                 (oneWayAt(tx, bottom) && previousBottom <= float(bottom) * kTile + 2.0f);
            if (blocked) {
                player_.y = float(bottom) * kTile - player_.h;
                player_.vy = 0.0f;
                landed = true;
            }
        }
        if (landed) {
            player_.onGround = true;
            player_.coyote = kCoyoteTime;
        }
    } else {
        const int top = static_cast<int>(std::floor(player_.y / kTile));
        for (int tx = left; tx <= right; ++tx) {
            if (solidAt(tx, top)) {
                player_.y = float(top + 1) * kTile;
                player_.vy = 0.0f;
                break;
            }
        }
    }
}

void World::hurt() {
    ++deaths_;
    player_.x = player_.respawnX;
    player_.y = player_.respawnY;
    player_.vx = 0.0f;
    player_.vy = 0.0f;
    player_.onGround = false;
    updateCamera();
}

void World::resolveEntityY(Entity& e, float dy) {
    e.y += dy;
    const float h = 26.0f;
    const float w = 26.0f;
    if (dy > 0.0f) {
        const int bottom = static_cast<int>(std::floor((e.y + h - 1.0f) / kTile));
        const int left = static_cast<int>(std::floor(e.x / kTile));
        const int right = static_cast<int>(std::floor((e.x + w - 1.0f) / kTile));
        for (int tx = left; tx <= right; ++tx) {
            if (solidAt(tx, bottom)) {
                e.y = float(bottom) * kTile - h;
                e.vy = 0.0f;
                break;
            }
        }
    }
}

void World::updateEntities(float dt) {
    for (auto& e : level_.entities) {
        if (!e.alive) continue;
        const EntityDef* def = content_->entity(e.type);
        if (!def) continue;

        if (e.type != "enemy.walker") continue;

        if (e.vx == 0.0f) e.vx = kEnemySpeed;
        e.vy = std::min(e.vy + kGravity * dt, kMaxFall);

        // horizontal patrol, bounce on walls
        e.x += e.vx * dt;
        if (rectHitsSolid(e.x + 3.0f, e.y + 3.0f, 20.0f, 20.0f)) {
            e.x -= e.vx * dt;
            e.vx = -e.vx;
        }
        resolveEntityY(e, e.vy * dt);

        // turn around at ledges so walkers don't march into pits forever
        const float probeX = e.vx > 0.0f ? e.x + 24.0f : e.x + 2.0f;
        const int probeTx = static_cast<int>(std::floor(probeX / kTile));
        const int probeTy = static_cast<int>(std::floor((e.y + 30.0f) / kTile));
        if (!solidAt(probeTx, probeTy) && !oneWayAt(probeTx, probeTy) && e.vy == 0.0f) {
            e.vx = -e.vx;
        }

        if (e.y > float(level_.h) * kTile + 200.0f) e.alive = false;
    }
}

void World::checkEntityTouches() {
    for (auto& e : level_.entities) {
        if (!e.alive) continue;
        const EntityDef* def = content_->entity(e.type);
        if (!def) continue;

        const float ew = float(def->w) * kTile;
        const float eh = float(def->h) * kTile;

        if (e.type == "enemy.walker") {
            const bool hit = overlaps(player_.x + 3.0f, player_.y + 3.0f, player_.w - 6.0f,
                                      player_.h - 6.0f, e.x + 3.0f, e.y + 3.0f, 20.0f, 20.0f);
            if (!hit) continue;
            const float playerBottom = player_.y + player_.h;
            const bool stomping = player_.vy > 0.0f && playerBottom < e.y + 18.0f;
            if (stomping) {
                e.alive = false;
                player_.vy = kStompBounce;
                player_.onGround = false;
            } else {
                hurt();
                return;
            }
            continue;
        }

        const bool touch = overlaps(player_.x, player_.y, player_.w, player_.h, e.x, e.y, ew, eh);
        if (!touch) continue;

        if (e.type == "coin") {
            e.alive = false;
            ++coins_;
        } else if (e.type == "checkpoint") {
            if (!e.touched) {
                e.touched = true;
                player_.respawnX = e.x + (ew - player_.w) * 0.5f;
                player_.respawnY = e.y + eh - player_.h;
            }
        } else if (e.type == "goal") {
            finished_ = true;
        }
    }
}

void World::updateCamera() {
    const float levelW = float(level_.w) * kTile;
    const float levelH = float(level_.h) * kTile;

    const float targetX = player_.x + player_.w * 0.5f - float(viewW_) * 0.5f;
    if (levelW <= float(viewW_)) {
        camera_.x = -(float(viewW_) - levelW) * 0.5f;
    } else {
        camera_.x = std::clamp(targetX, 0.0f, levelW - float(viewW_));
    }

    if (levelH <= float(viewH_)) {
        camera_.y = -(float(viewH_) - levelH) * 0.5f;
    } else {
        camera_.y = std::clamp(player_.y + player_.h * 0.5f - float(viewH_) * 0.55f, 0.0f,
                               levelH - float(viewH_));
    }
}

} // namespace pm
