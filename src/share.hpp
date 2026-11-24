#pragma once

#include <string>

#include "level.hpp"

namespace pm {

// Encodes a level as a short text code that fits in a caption or a chat
// message. Format: "PM1-" + base64url(packed binary).
//
// The payload is RLE-compressed tiles plus entity placements, no JSON and no
// zlib, so encoding never fails and never depends on a compression library.
// Unknown entity types are skipped; unknown tiles are already air.
std::string encodeLevelCode(const Level& level, const Content& content);

// Decodes a code produced by encodeLevelCode. Returns an empty string on
// success, or a human readable error.
std::string decodeLevelCode(const std::string& code, const Content& content, Level& out);

} // namespace pm
