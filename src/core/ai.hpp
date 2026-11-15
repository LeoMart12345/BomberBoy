#pragma once

#include <cstdint>
#include <random>

#include "core/game.hpp"

namespace bb {

/// Player two's computer opponent.
///
/// It plays with exactly the same rules as a human: it works out where the
/// bombs are dangerous, walks to a crate, drops a bomb only when it can still
/// get out of the blast, and otherwise keeps its distance. Every decision is
/// derived from the public `Game` state and the brain's own seeded
/// `std::mt19937`, so a seeded match replays identically and the self-test can
/// assert on its behaviour.
class AiBrain {
 public:
  /// Longest retreat, in tiles, the brain accepts before it dares to drop a
  /// bomb. A fuse burns for `Game::kBombFuse` seconds and a player covers more
  /// than two tiles in that time, so four steps is comfortable.
  static constexpr int kMaxEscapeSteps = 4;

  explicit AiBrain(int playerIndex = 1, std::uint32_t seed = 0x5eedu);

  /// One frame of input for `playerIndex`. `dt` only drives the stuck watchdog
  /// that frees the brain when its planned step keeps failing.
  PlayerInput think(const Game& game, float dt);

  /// Forgets the plan and the watchdog; call it when a new round starts.
  void reset();

 private:
  int m_playerIndex = 1;
  std::mt19937 m_rng;
  TileCoord m_lastTile{};
  float m_stuckTime = 0.f;

  /// The brain decides tile by tile. Once a step is chosen it keeps pressing
  /// that direction until the player actually reaches the next tile, which
  /// stops it from dithering while it straddles a tile boundary.
  bool m_hasPlan = false;
  TileCoord m_planTile{};
  TileCoord m_planDir{};
};

}  // namespace bb
