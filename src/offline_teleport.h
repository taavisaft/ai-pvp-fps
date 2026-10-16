#pragma once
#include "game.h"

// Offline exploration only. Bounded search, no allocation; false leaves the
// player unchanged when connected or no safe destination can be found.
bool teleportOfflinePlayer(GameState& game, bool connected, uint32_t& randomState);
