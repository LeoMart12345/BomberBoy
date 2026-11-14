#include "game.hpp"

#include <algorithm>
#include <cmath>

namespace bb {
namespace {

constexpr float kEpsilon = 1e-4f;

int randomInt(std::mt19937& rng, int low, int high) {
  std::uniform_int_distribution<int> dist(low, high);
  return dist(rng);
}

bool randomChance(std::mt19937& rng, float probability) {
  std::uniform_real_distribution<float> dist(0.f, 1.f);
  return dist(rng) < probability;
}

bool isHorizontal(Direction dir) {
  return dir == Direction::Left || dir == Direction::Right;
}

}  // namespace

Game::Game(std::uint32_t seed) : Game(seed, MatchRules{}) {}

Game::Game(std::uint32_t seed, const MatchRules& rules) : m_rng(seed), m_rules(rules) {
  m_rules.startingBombs = std::clamp(m_rules.startingBombs, 1, 8);
  m_rules.startingRange = std::clamp(m_rules.startingRange, 1, 9);
  m_rules.startingSpeed = std::clamp(m_rules.startingSpeed, 1.f, kMaxMoveSpeed);
  m_rules.roundsToWin = std::clamp(m_rules.roundsToWin, 1, 9);
  resetMatch();
}

TileCoord Game::spawnTile(int playerIndex) {
  if (playerIndex == 1) {
    return TileCoord{kCols - 2, kRows - 2};
  }
  return TileCoord{1, 1};
}

std::array<TileCoord, 2> Game::escapeLane(int playerIndex) {
  const TileCoord spawn = spawnTile(playerIndex);
  const int midX = kCols / 2;
  const int midY = kRows / 2;
  int stepX = 0;
  int stepY = 0;
  if (spawn.x != midX) {
    stepX = spawn.x < midX ? 1 : -1;
  } else {
    stepY = spawn.y < midY ? 1 : -1;
  }
  return {TileCoord{spawn.x + stepX, spawn.y + stepY},
          TileCoord{spawn.x + 2 * stepX, spawn.y + 2 * stepY}};
}

void Game::resetMatch() {
  m_scores = {0, 0};
  m_matchOver = false;
  m_matchWinner = -1;
  m_round = 0;
  m_players = {};
  resetRound();
}

void Game::resetRound() {
  m_bombs.clear();
  m_flames.clear();
  m_powerUps.clear();
  m_bombHeld = {false, false};
  m_state = RoundState::Playing;
  m_winner = -1;
  ++m_round;

  generateMap();

  for (int i = 0; i < kPlayerCount; ++i) {
    const TileCoord spawn = spawnTile(i);
    Player player;
    player.pos = Vec2{static_cast<float>(spawn.x) + 0.5f, static_cast<float>(spawn.y) + 0.5f};
    player.facing = i == 0 ? Direction::Down : Direction::Up;
    player.bombCapacity = m_rules.startingBombs;
    player.blastRange = m_rules.startingRange;
    player.moveSpeed = m_rules.startingSpeed;
    m_players[i] = player;
  }
}

void Game::generateMap() {
  m_tiles.fill(Tile::Floor);

  for (int y = 0; y < kRows; ++y) {
    for (int x = 0; x < kCols; ++x) {
      const bool border = x == 0 || y == 0 || x == kCols - 1 || y == kRows - 1;
      const bool pillar = x % 2 == 0 && y % 2 == 0;
      if (border || pillar) {
        m_tiles[y * kCols + x] = Tile::Wall;
      }
    }
  }

  for (int y = 1; y < kRows - 1; ++y) {
    for (int x = 1; x < kCols - 1; ++x) {
      if (m_tiles[y * kCols + x] != Tile::Floor) {
        continue;
      }
      // Each spawn keeps a three tile pocket open: the spawn tile itself plus the
      // two tile lane that leads towards the middle of the arena. Everything else
      // around a spawn can hold crates, so the opening bomb blows a crate apart
      // instead of fizzling in an empty plaza.
      bool safe = false;
      for (int i = 0; i < kPlayerCount; ++i) {
        const TileCoord spawn = spawnTile(i);
        const auto lane = escapeLane(i);
        if ((x == spawn.x && y == spawn.y) || (x == lane[0].x && y == lane[0].y) ||
            (x == lane[1].x && y == lane[1].y)) {
          safe = true;
          break;
        }
      }
      if (safe) {
        continue;
      }
      constexpr float kBrickChance = 0.72f;
      if (randomChance(m_rng, kBrickChance)) {
        m_tiles[y * kCols + x] = Tile::Brick;
      }
    }
  }

  // Put at least one crate right next to every spawn so the first bomb always has
  // something to break.
  static constexpr int kNeighbours[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
  for (int i = 0; i < kPlayerCount; ++i) {
    const TileCoord spawn = spawnTile(i);
    const auto lane = escapeLane(i);
    for (const auto& dir : kNeighbours) {
      const TileCoord tile{spawn.x + dir[0], spawn.y + dir[1]};
      if (!inBounds(tile.x, tile.y)) {
        continue;
      }
      if ((tile.x == lane[0].x && tile.y == lane[0].y) ||
          (tile.x == lane[1].x && tile.y == lane[1].y)) {
        continue;
      }
      if (m_tiles[static_cast<std::size_t>(tile.y * kCols + tile.x)] == Tile::Floor &&
          !(tile.x % 2 == 0 && tile.y % 2 == 0)) {
        m_tiles[static_cast<std::size_t>(tile.y * kCols + tile.x)] = Tile::Brick;
      }
    }
  }
}


void Game::update(float dt, const std::array<PlayerInput, kPlayerCount>& inputs) {
  dt = std::clamp(dt, 0.f, 0.05f);

  if (m_state == RoundState::RoundOver) {
    updateFlames(dt);
    return;
  }

  for (int i = 0; i < kPlayerCount; ++i) {
    Player& player = m_players[static_cast<std::size_t>(i)];
    if (!player.alive) {
      m_bombHeld[static_cast<std::size_t>(i)] = inputs[static_cast<std::size_t>(i)].dropBomb;
      continue;
    }
    if (inputs[static_cast<std::size_t>(i)].dropBomb && !m_bombHeld[static_cast<std::size_t>(i)]) {
      tryPlaceBomb(i);
    }
    m_bombHeld[static_cast<std::size_t>(i)] = inputs[static_cast<std::size_t>(i)].dropBomb;

    updatePlayer(i, inputs[static_cast<std::size_t>(i)], dt);
    collectPowerUps(player);
  }

  updateBombOccupancy();

  for (Bomb& bomb : m_bombs) {
    bomb.fuse -= dt;
  }

  detonateExpiredBombs();
  applyFlameDamage();
  updateFlames(dt);
  checkRoundEnd();
}

void Game::updatePlayer(int index, const PlayerInput& input, float dt) {
  Player& player = m_players[static_cast<std::size_t>(index)];

  float moveX = (input.right ? 1.f : 0.f) - (input.left ? 1.f : 0.f);
  float moveY = (input.down ? 1.f : 0.f) - (input.up ? 1.f : 0.f);

  // BomberBoy movement is strictly four-directional; when both axes are held the
  // axis the player is already facing wins so the controls stay predictable.
  if (moveX != 0.f && moveY != 0.f) {
    if (isHorizontal(player.facing)) {
      moveY = 0.f;
    } else {
      moveX = 0.f;
    }
  }
  if (moveX == 0.f && moveY == 0.f) {
    return;
  }

  if (moveX != 0.f) {
    player.facing = moveX < 0.f ? Direction::Left : Direction::Right;
  } else {
    player.facing = moveY < 0.f ? Direction::Up : Direction::Down;
  }

  const float distance = player.moveSpeed * dt;

  // Gently centre the player on the corridor they are walking through. Without
  // this nudging it is very easy to get caught on corners between tiles.
  if (moveX != 0.f) {
    const float target = std::floor(player.pos.y) + 0.5f;
    const float snap = std::clamp(target - player.pos.y, -distance * 1.5f, distance * 1.5f);
    const Vec2 snapped{player.pos.x, player.pos.y + snap};
    if (!collides(snapped, index)) {
      player.pos = snapped;
    }
  } else {
    const float target = std::floor(player.pos.x) + 0.5f;
    const float snap = std::clamp(target - player.pos.x, -distance * 1.5f, distance * 1.5f);
    const Vec2 snapped{player.pos.x + snap, player.pos.y};
    if (!collides(snapped, index)) {
      player.pos = snapped;
    }
  }

  // Advance in small sub-steps so fast players cannot tunnel through a wall.
  constexpr int kSteps = 6;
  const float step = distance / static_cast<float>(kSteps);
  for (int i = 0; i < kSteps; ++i) {
    Vec2 next = player.pos;
    next.x += moveX * step;
    next.y += moveY * step;
    if (collides(next, index)) {
      break;
    }
    player.pos = next;
  }
}

void Game::tryPlaceBomb(int index) {
  Player& player = m_players[static_cast<std::size_t>(index)];
  const TileCoord tile = centerTile(player.pos);

  if (!inBounds(tile.x, tile.y) || tileAt(tile.x, tile.y) != Tile::Floor) {
    return;
  }
  if (bombAt(tile) || player.activeBombs >= player.bombCapacity) {
    return;
  }

  Bomb bomb;
  bomb.tile = tile;
  bomb.owner = index;
  bomb.blastRange = player.blastRange;
  bomb.fuse = kBombFuse;
  m_bombs.push_back(bomb);
  ++player.activeBombs;
}

void Game::detonateExpiredBombs() {
  std::vector<std::size_t> pending;
  std::vector<bool> exploded(m_bombs.size(), false);

  for (std::size_t i = 0; i < m_bombs.size(); ++i) {
    if (m_bombs[i].fuse <= 0.f) {
      exploded[i] = true;
      pending.push_back(i);
    }
  }
  if (pending.empty()) {
    return;
  }

  static constexpr int kDirections[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

  while (!pending.empty()) {
    const std::size_t index = pending.back();
    pending.pop_back();
    const Bomb bomb = m_bombs[index];

    if (bomb.owner >= 0 && bomb.owner < kPlayerCount) {
      Player& owner = m_players[static_cast<std::size_t>(bomb.owner)];
      owner.activeBombs = std::max(0, owner.activeBombs - 1);
    }

    addFlame(bomb.tile, bomb.owner, false, true);

    for (const auto& dir : kDirections) {
      const bool horizontal = dir[0] != 0;
      for (int reach = 1; reach <= bomb.blastRange; ++reach) {
        const TileCoord tile{bomb.tile.x + dir[0] * reach, bomb.tile.y + dir[1] * reach};
        if (!inBounds(tile.x, tile.y)) {
          break;
        }

        const Tile tileType = tileAt(tile.x, tile.y);
        if (tileType == Tile::Wall) {
          break;
        }

        int chained = -1;
        if (bombAt(tile, &chained)) {
          addFlame(tile, bomb.owner, horizontal, false);
          if (!exploded[static_cast<std::size_t>(chained)]) {
            exploded[static_cast<std::size_t>(chained)] = true;
            pending.push_back(static_cast<std::size_t>(chained));
          }
          break;
        }

        addFlame(tile, bomb.owner, horizontal, false);

        if (tileType == Tile::Brick) {
          destroyBrick(tile);
          break;
        }
      }
    }
  }

  std::vector<Bomb> survivors;
  survivors.reserve(m_bombs.size());
  for (std::size_t i = 0; i < m_bombs.size(); ++i) {
    if (!exploded[i]) {
      survivors.push_back(m_bombs[i]);
    }
  }
  m_bombs.swap(survivors);
}

void Game::addFlame(TileCoord tile, int owner, bool horizontal, bool center) {
  Flame flame;
  flame.tile = tile;
  flame.owner = owner;
  flame.horizontal = horizontal;
  flame.center = center;
  flame.remaining = kFlameLifetime;
  flame.lifetime = kFlameLifetime;
  m_flames.push_back(flame);
}

void Game::destroyBrick(TileCoord tile) {
  if (tileAt(tile.x, tile.y) != Tile::Brick) {
    return;
  }
  m_tiles[static_cast<std::size_t>(tile.y * kCols + tile.x)] = Tile::Floor;

  constexpr float kPowerUpChance = 0.28f;
  if (!randomChance(m_rng, kPowerUpChance)) {
    return;
  }

  const int roll = randomInt(m_rng, 0, 99);
  PowerUpKind kind = PowerUpKind::ExtraBomb;
  if (roll >= 40 && roll < 80) {
    kind = PowerUpKind::ExtraRange;
  } else if (roll >= 80) {
    kind = PowerUpKind::Speed;
  }
  m_powerUps.push_back(PowerUp{tile, kind});
}

void Game::updateFlames(float dt) {
  for (Flame& flame : m_flames) {
    flame.remaining -= dt;
  }
  m_flames.erase(std::remove_if(m_flames.begin(), m_flames.end(),
                                [](const Flame& flame) { return flame.remaining <= 0.f; }),
                 m_flames.end());
}

void Game::applyFlameDamage() {
  for (Player& player : m_players) {
    if (!player.alive) {
      continue;
    }
    for (const Flame& flame : m_flames) {
      const float left = static_cast<float>(flame.tile.x);
      const float top = static_cast<float>(flame.tile.y);
      const float right = left + 1.f;
      const float bottom = top + 1.f;
      if (player.pos.x + kPlayerHalfSize > left && player.pos.x - kPlayerHalfSize < right &&
          player.pos.y + kPlayerHalfSize > top && player.pos.y - kPlayerHalfSize < bottom) {
        player.alive = false;
        break;
      }
    }
  }
}


void Game::collectPowerUps(Player& player) {
  for (auto it = m_powerUps.begin(); it != m_powerUps.end();) {
    const float left = static_cast<float>(it->tile.x);
    const float top = static_cast<float>(it->tile.y);
    const float right = left + 1.f;
    const float bottom = top + 1.f;
    const bool overlapping = player.pos.x + kPlayerHalfSize > left &&
                             player.pos.x - kPlayerHalfSize < right &&
                             player.pos.y + kPlayerHalfSize > top &&
                             player.pos.y - kPlayerHalfSize < bottom;
    if (!overlapping) {
      ++it;
      continue;
    }

    switch (it->kind) {
      case PowerUpKind::ExtraBomb:
        player.bombCapacity = std::min(player.bombCapacity + 1, 8);
        break;
      case PowerUpKind::ExtraRange:
        player.blastRange = std::min(player.blastRange + 1, 9);
        break;
      case PowerUpKind::Speed:
        player.moveSpeed = std::min(player.moveSpeed + 0.5f, kMaxMoveSpeed);
        break;
    }
    it = m_powerUps.erase(it);
  }
}

void Game::checkRoundEnd() {
  if (m_state == RoundState::RoundOver) {
    return;
  }

  int aliveCount = 0;
  int lastAlive = -1;
  for (int i = 0; i < kPlayerCount; ++i) {
    if (m_players[static_cast<std::size_t>(i)].alive) {
      ++aliveCount;
      lastAlive = i;
    }
  }
  if (aliveCount > 1) {
    return;
  }

  m_state = RoundState::RoundOver;
  m_winner = aliveCount == 1 ? lastAlive : -1;
  if (m_winner >= 0) {
    ++m_scores[static_cast<std::size_t>(m_winner)];
    if (m_scores[static_cast<std::size_t>(m_winner)] >= m_rules.roundsToWin) {
      m_matchOver = true;
      m_matchWinner = m_winner;
    }
  }
}

bool Game::collides(const Vec2& center, int playerIndex) const {
  const float left = center.x - kPlayerHalfSize + kEpsilon;
  const float right = center.x + kPlayerHalfSize - kEpsilon;
  const float top = center.y - kPlayerHalfSize + kEpsilon;
  const float bottom = center.y + kPlayerHalfSize - kEpsilon;

  const int x0 = static_cast<int>(std::floor(left));
  const int x1 = static_cast<int>(std::floor(right));
  const int y0 = static_cast<int>(std::floor(top));
  const int y1 = static_cast<int>(std::floor(bottom));

  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      if (isSolidForPlayer(TileCoord{x, y}, playerIndex)) {
        return true;
      }
    }
  }
  return false;
}

bool Game::isSolidForPlayer(TileCoord tile, int playerIndex) const {
  if (!inBounds(tile.x, tile.y)) {
    return true;
  }
  if (tileAt(tile.x, tile.y) != Tile::Floor) {
    return true;
  }

  int bombIndex = -1;
  if (bombAt(tile, &bombIndex)) {
    const Bomb& bomb = m_bombs[static_cast<std::size_t>(bombIndex)];
    // The player who dropped a bomb may walk off it, but only until their body
    // has fully cleared the tile - after that it is as solid as any other.
    return !(playerIndex >= 0 && bomb.owner == playerIndex && !bomb.ownerCleared);
  }
  return false;
}

void Game::updateBombOccupancy() {
  const float half = kPlayerHalfSize - kEpsilon;
  for (Bomb& bomb : m_bombs) {
    if (bomb.ownerCleared) {
      continue;
    }
    if (bomb.owner < 0 || bomb.owner >= kPlayerCount) {
      bomb.ownerCleared = true;
      continue;
    }

    const Player& owner = m_players[static_cast<std::size_t>(bomb.owner)];
    const int x0 = static_cast<int>(std::floor(owner.pos.x - half));
    const int x1 = static_cast<int>(std::floor(owner.pos.x + half));
    const int y0 = static_cast<int>(std::floor(owner.pos.y - half));
    const int y1 = static_cast<int>(std::floor(owner.pos.y + half));

    if (bomb.tile.x < x0 || bomb.tile.x > x1 || bomb.tile.y < y0 || bomb.tile.y > y1) {
      bomb.ownerCleared = true;
    }
  }
}

bool Game::bombAt(TileCoord tile, int* bombIndex) const {
  for (std::size_t i = 0; i < m_bombs.size(); ++i) {
    if (m_bombs[i].tile == tile) {
      if (bombIndex != nullptr) {
        *bombIndex = static_cast<int>(i);
      }
      return true;
    }
  }
  return false;
}

TileCoord Game::centerTile(const Vec2& pos) const {
  return TileCoord{static_cast<int>(std::floor(pos.x)), static_cast<int>(std::floor(pos.y))};
}

Tile Game::tileAt(int x, int y) const {
  if (!inBounds(x, y)) {
    return Tile::Wall;
  }
  return m_tiles[static_cast<std::size_t>(y * kCols + x)];
}

bool Game::inBounds(int x, int y) const {
  return x >= 0 && y >= 0 && x < kCols && y < kRows;
}

}  // namespace bb

