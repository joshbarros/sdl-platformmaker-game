#pragma once

#include <map>
#include <string>
#include <vector>

namespace pm {

// A tiny persistent profile: counters that survive restarts and drive both the
// achievements and the daily streak. Everything is plain JSON in the user's
// prefs folder so it can be inspected (and modded) by hand.
struct Profile {
    std::map<std::string, int> counters;
    std::map<std::string, bool> unlocked;

    std::string dailyDate;   // last daily level played (YYYY-MM-DD)
    int dailyStreak = 0;
    float dailyBest = 0.0f;  // best clear time for that date

    bool dirty = false;

    int count(const std::string& key) const;
    void bump(const std::string& key, int amount = 1);

    static Profile load(const std::string& path);
    bool save(const std::string& path) const;
};

struct Achievement {
    const char* id;
    const char* name;
    const char* description;
    const char* counter;   // counter that must reach threshold
    int threshold;
};

const std::vector<Achievement>& achievements();

// Returns the achievements newly unlocked by the current counters. Mutates the
// profile (unlocked map) and returns their display names.
std::vector<std::string> evaluateAchievements(Profile& profile);

std::string todayKey();                 // UTC date, YYYY-MM-DD
std::string dateKeyOffset(int daysAgo); // for streak validation
int daysBetween(const std::string& a, const std::string& b);

} // namespace pm
