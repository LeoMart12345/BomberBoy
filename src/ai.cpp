#include "ai.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

namespace bb {
namespace {

constexpr int kTiles = kCols * kRows;
using Grid = std::array<bool, kTiles>;
using TimeGrid = std::array<float, kTiles>;
constexpr float kNever = std::numeric_limits<float>::infinity();

/// Slack added to every estimated walk time: frames are not free and a fuse that
/// only just outlasts the walk is no escape at all.
constexpr float kEscapeMargin = 0.12f;

constexpr std::array<TileCoord, 4> kNeighbours = {TileCoord{0, -1}, TileCoord{0, 1},
                                                 TileCoord{-1, 0}, TileCoord{1, 0}};

std::size_t gridIndex(TileCoord tile) {
  return static_cast<std::size_t>(tile.y * kCols + tile.x);
}

TileCoord tileOf(const Vec2& pos) {
  return TileCoord{static_cast<int>(std::floor(pos.x)), static_cast<int>(std::floor(pos.y))};
}

bool isFloor(const Game& game, TileCoord tile) {
  return game.inBounds(tile.x, tile.y) && game.tileAt(tile.x, tile.y) == Tile::Floor;
}

bool bombAt(const Game& game, TileCoord tile) {
  return std::any_of(game.bombs().begin(), game.bombs().end(),
                     [tile](const Bomb& bomb) { return bomb.tile == tile; });
}

int blastRangeAt(const Game& game, TileCoord tile) {
  for (const Bomb& bomb : game.bombs()) {
    if (bomb.tile == tile) {
      return bomb.blastRange;
    }
  }
  return 0;
}

/// Walks the tiles one explosion covers. `visit` is called for the bomb tile
/// itself and then along every arm; arms stop at walls and at the first crate,
/// which burns and stops them. Arms are followed through bombs as well, which
/// the game stops at: over-predicting a blast is safe, missing one is not.
template <typename Visit>
void forEachBlastTile(const Game& game, TileCoord origin, int range, Visit visit) {
  visit(origin);

  for (const TileCoord step : kNeighbours) {
    for (int distance = 1; distance <= range; ++distance) {
      const TileCoord tile{origin.x + step.x * distance, origin.y + step.y * distance};
      if (!game.inBounds(tile.x, tile.y)) {
        break;
      }
      if (game.tileAt(tile.x, tile.y) == Tile::Wall) {
        break;
      }

      visit(tile);

      if (game.tileAt(tile.x, tile.y) == Tile::Brick) {
        break;
      }
    }
  }
}

/// Marks the arms of one explosion. Arms stop at walls and at the first crate,
/// and every bomb they touch is appended to `touched` so the caller can follow
/// the chain.
void markArms(const Game& game, TileCoord origin, int range, Grid& blast,
              std::vector<TileCoord>* touched) {
  forEachBlastTile(game, origin, range, [&](TileCoord tile) {
    blast[gridIndex(tile)] = true;
    if (touched != nullptr && tile != origin && bombAt(game, tile)) {
      touched->push_back(tile);
    }
  });
}

/// Where an explosion hurts. `lethal` is burning right now and must never be
/// entered; `predicted` also covers the arms of live bombs, which a bomber is
/// allowed to run through on the way out but should not linger in.
struct Hazards {
  Grid lethal{};
  Grid predicted{};

  bool safe(TileCoord tile) const { return !predicted[gridIndex(tile)]; }
};

/// Collects the flames and the arms of every live bomb, following chains.
Hazards hazards(const Game& game) {
  Hazards result;
  for (const Flame& flame : game.flames()) {
    result.lethal[gridIndex(flame.tile)] = true;
  }
  result.predicted = result.lethal;

  std::array<bool, kTiles> queued{};
  std::vector<TileCoord> pending;
  const auto enqueue = [&queued, &pending](TileCoord tile) {
    if (!queued[gridIndex(tile)]) {
      queued[gridIndex(tile)] = true;
      pending.push_back(tile);
    }
  };

  for (const Bomb& bomb : game.bombs()) {
    enqueue(bomb.tile);
  }

  std::vector<TileCoord> touched;
  while (!pending.empty()) {
    const TileCoord origin = pending.back();
    pending.pop_back();
    touched.clear();
    markArms(game, origin, blastRangeAt(game, origin), result.predicted, &touched);
    for (const TileCoord tile : touched) {
      enqueue(tile);
    }
  }

  return result;
}

/// Earliest time each tile starts burning, following bomb chains. Tiles no bomb
/// can reach keep `kNever`. Knowing when a tile burns - not just whether it will
/// - lets the brain cross the arms of the bomb it has just dropped while
/// refusing the arms of a bomb whose fuse is nearly out.
TimeGrid burnTimes(const Game& game) {
  TimeGrid burn;
  burn.fill(kNever);

  for (const Flame& flame : game.flames()) {
    burn[gridIndex(flame.tile)] = 0.f;
  }

  const auto& bombs = game.bombs();
  std::vector<float> when;
  when.reserve(bombs.size());
  for (const Bomb& bomb : bombs) {
    when.push_back(bomb.fuse);
  }
  std::vector<bool> settled(bombs.size(), false);

  // Always expand the bomb that goes off first: a bomb touched by an earlier
  // blast is triggered by it, so its own fuse no longer matters.
  for (;;) {
    std::size_t next = bombs.size();
    for (std::size_t index = 0; index < bombs.size(); ++index) {
      if (!settled[index] && (next == bombs.size() || when[index] < when[next])) {
        next = index;
      }
    }
    if (next == bombs.size()) {
      break;
    }
    settled[next] = true;

    const Bomb& bomb = bombs[next];
    forEachBlastTile(game, bomb.tile, bomb.blastRange, [&](TileCoord tile) {
      burn[gridIndex(tile)] = std::min(burn[gridIndex(tile)], when[next]);
      for (std::size_t other = 0; other < bombs.size(); ++other) {
        if (!settled[other] && bombs[other].tile == tile) {
          when[other] = std::min(when[other], when[next]);
        }
      }
    });
  }

  return burn;
}

/// Tiles the brain refuses to walk on: anything that is not open floor, live
/// bombs and everything the supplied hazard map marks as lethal.
Grid blockedTiles(const Game& game, const Grid& hazard) {
  Grid blocked{};
  for (int y = 0; y < kRows; ++y) {
    for (int x = 0; x < kCols; ++x) {
      const TileCoord tile{x, y};
      blocked[gridIndex(tile)] =
          !isFloor(game, tile) || bombAt(game, tile) || hazard[gridIndex(tile)];
    }
  }
  return blocked;
}

struct Path {
  bool found = false;
  TileCoord firstStep{};
  int steps = 0;  // moves needed to reach the goal
};

using Goal = std::function<bool(TileCoord)>;

/// Breadth first search over open tiles. `blocked` may mark the start tile
/// (the brain often starts on a tile it has to leave), so the start itself is
/// never treated as a goal and never tested against `blocked`.
Path search(const Game& game, TileCoord start, const Grid& blocked, const Goal& isGoal) {
  Path path;
  if (!game.inBounds(start.x, start.y)) {
    return path;
  }

  std::array<int, kTiles> cameFrom;
  cameFrom.fill(-1);
  std::array<bool, kTiles> seen{};
  seen[gridIndex(start)] = true;

  std::vector<TileCoord> queue{start};
  for (std::size_t head = 0; head < queue.size(); ++head) {
    const TileCoord current = queue[head];
    if (current != start && isGoal(current)) {
      std::vector<TileCoord> chain{current};
      while (true) {
        const int previous = cameFrom[gridIndex(chain.back())];
        if (previous < 0) {
          break;
        }
        chain.push_back(TileCoord{previous % kCols, previous / kCols});
      }

      path.found = true;
      path.steps = static_cast<int>(chain.size()) - 1;
      path.firstStep = path.steps > 0 ? chain[chain.size() - 2] : start;
      return path;
    }

    for (const TileCoord step : kNeighbours) {
      const TileCoord next{current.x + step.x, current.y + step.y};
      if (!game.inBounds(next.x, next.y)) {
        continue;
      }
      if (seen[gridIndex(next)] || blocked[gridIndex(next)]) {
        continue;
      }
      seen[gridIndex(next)] = true;
      cameFrom[gridIndex(next)] = current.y * kCols + current.x;
      queue.push_back(next);
    }
  }

  return path;
}

/// Escape search for a brain that is standing in a blast. Unlike `search` it
/// does not refuse blast tiles outright - a bomber has to cross the arms of its
/// own bomb to leave a corridor - but it only steps onto a tile when the body
/// can be off it again before the blast arrives. Goals are tiles no bomb can
/// ever reach.
Path timedEscape(const Game& game, TileCoord start, const Grid& blocked, const TimeGrid& burn,
                 float secondsPerTile) {
  Path path;
  if (!game.inBounds(start.x, start.y)) {
    return path;
  }

  std::array<int, kTiles> cameFrom;
  cameFrom.fill(-1);
  std::array<int, kTiles> distance;
  distance.fill(-1);
  distance[gridIndex(start)] = 0;

  std::vector<TileCoord> queue{start};
  for (std::size_t head = 0; head < queue.size(); ++head) {
    const TileCoord current = queue[head];
    const int here = distance[gridIndex(current)];

    if (current != start && !std::isfinite(burn[gridIndex(current)])) {
      std::vector<TileCoord> chain{current};
      while (true) {
        const int previous = cameFrom[gridIndex(chain.back())];
        if (previous < 0) {
          break;
        }
        chain.push_back(TileCoord{previous % kCols, previous / kCols});
      }

      path.found = true;
      path.steps = static_cast<int>(chain.size()) - 1;
      path.firstStep = path.steps > 0 ? chain[chain.size() - 2] : start;
      return path;
    }

    for (const TileCoord step : kNeighbours) {
      const TileCoord next{current.x + step.x, current.y + step.y};
      if (!game.inBounds(next.x, next.y)) {
        continue;
      }
      if (distance[gridIndex(next)] >= 0 || blocked[gridIndex(next)]) {
        continue;
      }

      // One more step is needed before the body leaves `next` again, so the
      // blast has to arrive after that, with a little room to spare: the brain
      // takes a frame or two to react, and a fuse that only just outlasts the
      // walk can still catch it.
      const float leaveTime = static_cast<float>(here + 2) * secondsPerTile + kEscapeMargin;
      if (std::isfinite(burn[gridIndex(next)]) && leaveTime > burn[gridIndex(next)]) {
        continue;
      }

      distance[gridIndex(next)] = here + 1;
      cameFrom[gridIndex(next)] = current.y * kCols + current.x;
      queue.push_back(next);
    }
  }

  return path;
}

PlayerInput steer(TileCoord from, TileCoord to) {
  PlayerInput input;
  if (to.x < from.x) {
    input.left = true;
  } else if (to.x > from.x) {
    input.right = true;
  } else if (to.y < from.y) {
    input.up = true;
  } else if (to.y > from.y) {
    input.down = true;
  }
  return input;
}

bool nextToCrate(const Game& game, TileCoord tile) {
  return std::any_of(kNeighbours.begin(), kNeighbours.end(), [&game, tile](TileCoord step) {
    const TileCoord next{tile.x + step.x, tile.y + step.y};
    return game.inBounds(next.x, next.y) && game.tileAt(next.x, next.y) == Tile::Brick;
  });
}

/// A pickup lying on the floor, ready to be walked over.
bool powerUpAt(const Game& game, TileCoord tile) {
  return std::any_of(game.powerUps().begin(), game.powerUps().end(),
                     [tile](const PowerUp& powerUp) { return powerUp.tile == tile; });
}

bool opponentInBlast(const Game& game, int self, const Grid& blast) {
  const auto& players = game.players();
  for (int index = 0; index < kPlayerCount; ++index) {
    if (index == self || !players[static_cast<std::size_t>(index)].alive) {
      continue;
    }
    const TileCoord tile = tileOf(players[static_cast<std::size_t>(index)].pos);
    if (game.inBounds(tile.x, tile.y) && blast[gridIndex(tile)]) {
      return true;
    }
  }
  return false;
}

/// Least-bad move when no escape exists: step onto the neighbour the blast
/// reaches last, and stay put when the tile underfoot is the last to burn. Never
/// picks a tile that burns sooner than standing still - a random step into a
/// blast is how a cornered bomber kills itself.
PlayerInput retreat(const Game& game, TileCoord here, const Grid& blocked, const TimeGrid& burn) {
  float best = burn[gridIndex(here)];
  TileCoord target{};
  bool found = false;
  for (const TileCoord step : kNeighbours) {
    const TileCoord next{here.x + step.x, here.y + step.y};
    if (!game.inBounds(next.x, next.y) || blocked[gridIndex(next)]) {
      continue;
    }
    if (burn[gridIndex(next)] > best) {
      best = burn[gridIndex(next)];
      target = next;
      found = true;
    }
  }
  return found ? steer(here, target) : PlayerInput{};
}

PlayerInput wander(TileCoord here, const Grid& blocked, std::mt19937& rng) {
  std::array<TileCoord, 4> open{};
  int count = 0;
  for (const TileCoord step : kNeighbours) {
    const TileCoord next{here.x + step.x, here.y + step.y};
    if (next.x < 0 || next.y < 0 || next.x >= kCols || next.y >= kRows) {
      continue;
    }
    if (blocked[gridIndex(next)]) {
      continue;
    }
    open[static_cast<std::size_t>(count++)] = next;
  }

  if (count == 0) {
    return {};
  }

  std::uniform_int_distribution<int> pick(0, count - 1);
  return steer(here, open[static_cast<std::size_t>(pick(rng))]);
}

}  // namespace


AiBrain::AiBrain(int playerIndex, std::uint32_t seed) : m_playerIndex(playerIndex), m_rng(seed) {
  reset();
}

void AiBrain::reset() {
  m_lastTile = TileCoord{-1, -1};
  m_stuckTime = 0.f;
  m_hasPlan = false;
  m_planTile = TileCoord{};
  m_planDir = TileCoord{};
}

PlayerInput AiBrain::think(const Game& game, float dt) {
  PlayerInput input;
  if (game.state() != RoundState::Playing) {
    return input;
  }
  if (m_playerIndex < 0 || m_playerIndex >= kPlayerCount) {
    return input;
  }

  const Player& me = game.players()[static_cast<std::size_t>(m_playerIndex)];
  if (!me.alive) {
    return input;
  }

  const TileCoord here = tileOf(me.pos);
  if (here == m_lastTile) {
    m_stuckTime += dt;
  } else {
    m_lastTile = here;
    m_stuckTime = 0.f;
  }

  const Hazards hazard = hazards(game);
  const TimeGrid burn = burnTimes(game);
  const Grid lethalBlocked = blockedTiles(game, hazard.lethal);
  const Grid safeBlocked = blockedTiles(game, hazard.predicted);

  // Walking takes a little longer than `moveSpeed` suggests because the brain
  // stops on every tile centre before it plans the next step. The estimate is
  // deliberately pessimistic: guessing too fast walks into an explosion.
  const float secondsPerTile = 1.f / std::max(0.5f, me.moveSpeed * 0.85f);

  // The game damages whatever tile the player's body box touches, so a bomber
  // that stops halfway between two tiles can still be hit by a blast covering
  // the tile it just left. Decisions are therefore only taken from the middle
  // of a tile, and a committed step stays pressed until the body is centred in
  // the next one.
  const auto stepTile = [this] {
    return TileCoord{m_planTile.x + m_planDir.x, m_planTile.y + m_planDir.y};
  };
  const auto centredOn = [&me, dt](const Vec2& pos, TileCoord tile) {
    // A step can overshoot the tile centre by about one frame of travel. The
    // tolerance is capped below 0.16 so a resting body box always stays inside
    // its own tile, even when a slow frame makes the bomber move further.
    const float tolerance = std::clamp(me.moveSpeed * dt * 0.75f, 0.06f, 0.15f);
    return std::fabs(pos.x - (static_cast<float>(tile.x) + 0.5f)) <= tolerance &&
           std::fabs(pos.y - (static_cast<float>(tile.y) + 0.5f)) <= tolerance;
  };
  const auto inStep = [this, here] { return m_hasPlan && here == m_planTile; };
  const auto pressPlan = [this] {
    PlayerInput input;
    input.left = m_planDir.x < 0;
    input.right = m_planDir.x > 0;
    input.up = m_planDir.y < 0;
    input.down = m_planDir.y > 0;
    return input;
  };
  const auto commit = [this, here](TileCoord next) {
    m_hasPlan = true;
    m_planTile = here;
    m_planDir = TileCoord{next.x - here.x, next.y - here.y};
    return steer(here, next);
  };
  const auto forget = [this] { m_hasPlan = false; };
  // Walking back to the middle of the current tile is always safe for the body
  // box and stops the brain from hesitating on a tile boundary.
  const auto recentre = [&me, &centredOn](TileCoord tile) {
    PlayerInput input;
    if (centredOn(me.pos, tile)) {
      return input;
    }
    const float centreX = static_cast<float>(tile.x) + 0.5f;
    const float centreY = static_cast<float>(tile.y) + 0.5f;
    if (std::fabs(me.pos.x - centreX) >= std::fabs(me.pos.y - centreY)) {
      input.left = me.pos.x > centreX;
      input.right = me.pos.x < centreX;
    } else {
      input.up = me.pos.y > centreY;
      input.down = me.pos.y < centreY;
    }
    return input;
  };

  // Being inside a blast beats every other plan: leave first, bomb later. The
  // escape may run through tiles the blast will cover - that is how a corridor
  // works - but it has to end on a tile outside every bomb.
  if (!hazard.safe(here)) {
    // Keep the running step as long as it does not walk into live flames.
    if (inStep() && !hazard.lethal[gridIndex(stepTile())]) {
      return pressPlan();
    }
    const Path escape = timedEscape(game, here, lethalBlocked, burn, secondsPerTile);
    if (escape.found) {
      return commit(escape.firstStep);
    }
    return retreat(game, here, lethalBlocked, burn);
  }

  // A step that makes no progress at all: give up on it and try any safe one.
  if (m_stuckTime > 1.f) {
    m_stuckTime = 0.f;
    forget();
    return wander(here, safeBlocked, m_rng);
  }

  // Keep walking the committed step until the next tile is reached.
  if (inStep()) {
    if (hazard.safe(stepTile())) {
      return pressPlan();
    }
    forget();
  }

  // Only plan from a tile centre so the body box never pokes into the tile
  // behind while the brain believes it is standing somewhere safe.
  if (!centredOn(me.pos, here)) {
    return recentre(here);
  }

  // Drop a bomb when it is worth it and there is still a way out.
  if (me.activeBombs < me.bombCapacity && !bombAt(game, here)) {
    // Crossing the arms of the bomb that is about to be dropped is exactly how
    // a bomber leaves a corridor, so those tiles get the fresh fuse. Arms of
    // bombs that are already ticking keep their real, shorter fuse: a corridor
    // that burns before the brain can cross it is not an escape route.
    Grid prospective = hazard.predicted;
    TimeGrid escapeBurn = burn;
    forEachBlastTile(game, here, me.blastRange, [&](TileCoord tile) {
      prospective[gridIndex(tile)] = true;
      escapeBurn[gridIndex(tile)] = std::min(escapeBurn[gridIndex(tile)], Game::kBombFuse);
    });

    const bool worthIt =
        nextToCrate(game, here) || opponentInBlast(game, m_playerIndex, prospective);
    if (worthIt) {
      const Path escape = timedEscape(game, here, lethalBlocked, escapeBurn, secondsPerTile);
      if (escape.found && escape.steps <= kMaxEscapeSteps) {
        // Keep the escape the drop was checked against. Planning again on the
        // very next frame can reject the only route once the fuse has ticked
        // down, and a cornered bomber that turns around dies.
        input.dropBomb = true;
        const PlayerInput move = commit(escape.firstStep);
        input.left = move.left;
        input.right = move.right;
        input.up = move.up;
        input.down = move.down;
        return input;
      }
    }
  }

  Path path =
      search(game, here, safeBlocked, [&game](TileCoord tile) { return nextToCrate(game, tile); });
  // Nothing left to blow up within reach: grab a pickup before hunting the other
  // player. The pickup tile is a goal like any other, so the walk there stays on
  // tiles no blast can reach.
  if (!path.found) {
    path = search(game, here, safeBlocked,
                  [&game](TileCoord tile) { return powerUpAt(game, tile); });
  }
  if (!path.found) {
    const auto& players = game.players();
    const auto other = static_cast<std::size_t>(m_playerIndex == 0 ? 1 : 0);
    const TileCoord enemy = tileOf(players[other].pos);
    path = search(game, here, safeBlocked, [enemy](TileCoord tile) { return tile == enemy; });
  }
  if (path.found && path.firstStep != here) {
    return commit(path.firstStep);
  }

  forget();
  return wander(here, safeBlocked, m_rng);
}

}  // namespace bb
