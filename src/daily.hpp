#pragma once

#include <string>

#include "level.hpp"

namespace pm {

// A daily level generated from the UTC date. Same seed for everyone in the
// world that day, no server needed: that is the whole trick.
Level makeDailyLevel(const Content& content, const std::string& dateKey);

} // namespace pm
