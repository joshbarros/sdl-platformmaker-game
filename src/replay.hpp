#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "world.hpp"

namespace pm {

// An input log for one run. Because the simulation is fixed-step and has no
// randomness, replaying these bits reproduces the run exactly: that is the
// foundation for ghost runs, verified records and shareable clips.
struct Replay {
    std::string levelCode;
    int steps = 0;
    std::vector<std::uint8_t> bits; // 4 bits per step: left, right, jump, run
};

class ReplayRecorder {
public:
    void reset(const std::string& levelCode);
    void step(const InputState& input);
    bool empty() const { return replay_.steps == 0; }
    const Replay& replay() const { return replay_; }

private:
    Replay replay_;
};

class ReplayPlayer {
public:
    explicit ReplayPlayer(Replay replay);
    InputState next();
    bool done() const { return index_ >= replay_.steps; }
    int stepIndex() const { return index_; }

private:
    Replay replay_;
    int index_ = 0;
    bool previousJump_ = false;
};

std::string encodeReplay(const Replay& replay); // JSON text
bool decodeReplay(const std::string& text, Replay& out);
bool saveReplay(const std::string& path, const Replay& replay);
bool loadReplay(const std::string& path, Replay& out);

} // namespace pm
