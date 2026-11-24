#include "profile.hpp"

#include <cmath>
#include <ctime>
#include <fstream>

#include <nlohmann/json.hpp>

namespace pm {
namespace {

const std::vector<Achievement> kAchievements = {
    {"first_coin", "Pocket Change", "Collect your first coin", "coins", 1},
    {"coins_50", "Coin Purse", "Collect 50 coins", "coins", 50},
    {"first_stomp", "Squash", "Stomp your first enemy", "stomps", 1},
    {"stomps_25", "Exterminator", "Stomp 25 enemies", "stomps", 25},
    {"first_clear", "Explorer", "Clear a level", "clears", 1},
    {"flawless", "Flawless", "Clear a level without dying", "flawless", 1},
    {"speedrun", "Speedrunner", "Clear a level under 45 seconds", "fast_clears", 1},
    {"deaths_10", "Persistent", "Die 10 times (and keep going)", "deaths", 10},
    {"share_code", "Spreader", "Copy a level share code", "shares", 1},
    {"builder", "Architect", "Place 100 tiles in the editor", "tiles_painted", 100},
    {"daily_3", "Habit", "Keep a 3 day daily streak", "daily_streak", 3},
    {"daily_7", "Devoted", "Keep a 7 day daily streak", "daily_streak", 7},
};

std::time_t toUtc(std::tm& t) {
#if defined(_WIN32)
    return _mkgmtime(&t);
#else
    return timegm(&t);
#endif
}

std::time_t utcMidnight(int daysAgo) {
    std::time_t now = std::time(nullptr);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    utc.tm_hour = 12;
    utc.tm_min = 0;
    utc.tm_sec = 0;
    std::time_t noon = toUtc(utc);
    return noon - std::time_t(daysAgo) * 86400;
}

std::string keyFromTime(std::time_t t) {
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &t);
#else
    gmtime_r(&t, &utc);
#endif
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", utc.tm_year + 1900, utc.tm_mon + 1,
                  utc.tm_mday);
    return buffer;
}

} // namespace

int Profile::count(const std::string& key) const {
    auto it = counters.find(key);
    return it == counters.end() ? 0 : it->second;
}

void Profile::bump(const std::string& key, int amount) {
    counters[key] += amount;
    dirty = true;
}

Profile Profile::load(const std::string& path) {
    Profile profile;
    std::ifstream in(path);
    if (!in) return profile;

    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception&) {
        return profile;
    }
    if (j.contains("counters")) {
        for (auto& [k, v] : j["counters"].items()) profile.counters[k] = v.get<int>();
    }
    if (j.contains("unlocked")) {
        for (auto& [k, v] : j["unlocked"].items()) profile.unlocked[k] = v.get<bool>();
    }
    profile.dailyDate = j.value("dailyDate", "");
    profile.dailyStreak = j.value("dailyStreak", 0);
    profile.dailyBest = j.value("dailyBest", 0.0f);
    return profile;
}

bool Profile::save(const std::string& path) const {
    nlohmann::json j;
    j["counters"] = counters;
    j["unlocked"] = unlocked;
    j["dailyDate"] = dailyDate;
    j["dailyStreak"] = dailyStreak;
    j["dailyBest"] = dailyBest;

    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << j.dump(2) << "\n";
    return true;
}

const std::vector<Achievement>& achievements() {
    return kAchievements;
}

std::vector<std::string> evaluateAchievements(Profile& profile) {
    std::vector<std::string> unlockedNow;
    for (const auto& a : kAchievements) {
        if (profile.unlocked[a.id]) continue;
        if (profile.count(a.counter) < a.threshold) continue;
        profile.unlocked[a.id] = true;
        profile.dirty = true;
        unlockedNow.emplace_back(a.name);
    }
    return unlockedNow;
}

std::string todayKey() {
    return keyFromTime(utcMidnight(0));
}

std::string dateKeyOffset(int daysAgo) {
    return keyFromTime(utcMidnight(daysAgo));
}

int daysBetween(const std::string& a, const std::string& b) {
    auto parse = [](const std::string& s) {
        std::tm tm{};
        if (std::sscanf(s.c_str(), "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) != 3) {
            return std::time_t(0);
        }
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
        tm.tm_hour = 12;
        return toUtc(tm);
    };
    const double diff = std::difftime(parse(b), parse(a));
    return int(std::lround(diff / 86400.0));
}

} // namespace pm
