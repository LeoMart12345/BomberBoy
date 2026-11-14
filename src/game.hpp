#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace bb {

constexpr int kCols = 15;
constexpr int kRows = 13;
constexpr int kPlayerCount = 2;

enum class Tile : std::uint8_t { Floor, Wall, Brick };

enum class PowerUpKind : std::uint8_t { ExtraBomb, ExtraRange, Speed };

enum class Direction : std::uint8_t { Up, Down, Left, Right };

enum class RoundState : std::uint8_t { Playing, RoundOver };

struct Vec2 {
  float x = 0.f;
  float y = 0.f;
};

struct TileCoord {
  int x = 0;
  int y = 0;
};

inline bool operator==(TileCoord a, TileCoord b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(TileCoord a, TileCoord b) { return !(a == b); }

struct PlayerInput {
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  bool dropBomb = false;
};

struct Player {
  Vec2 pos{};  // centre of the player, measured in tiles
  Direction facing = Direction::Down;
  int activeBombs = 0;
  int bombCapacity = 1;
  int blastRange = 1;
  float moveSpeed = 3.6f;  // tiles per second
  bool alive = true;
};

/// Tweaks the loadout every round starts with. The defaults match the classic game.
struct MatchRules {
  int startingBombs = 1;
  int startingRange = 1;
  float startingSpeed = 3.6f;
  int roundsToWin = 3;
};

struct Bomb {
  TileCoord tile{};
  int owner = 0;
  int blastRange = 2;
  float fuse = 0.f;  // seconds left until it blows up
  /// Set once the owner's body has fully stepped off the tile; from then on the
  /// bomb blocks them like it blocks everyone else.
  bool ownerCleared = false;
};

struct Flame {
  TileCoord tile{};
  int owner = 0;
  float remaining = 0.f;
  float lifetime = 0.f;
  bool horizontal = false;  // used to stretch the arm visually
  bool center = false;      // the tile the bomb exploded on
};

struct PowerUp {
  TileCoord tile{};
  PowerUpKind kind = PowerUpKind::ExtraBomb;
};

/// Headless BomberBoy match logic: placement, explosions, power-ups and rounds.
class Game {
 public:
  static constexpr float kBombFuse = 2.4f;
  static constexpr float kFlameLifetime = 0.45f;
  static constexpr float kPlayerHalfSize = 0.34f;  // in tiles
  static constexpr float kMaxMoveSpeed = 6.6f;

  explicit Game(std::uint32_t seed = std::random_device{}());
  Game(std::uint32_t seed, const MatchRules& rules);

  /// Builds a brand new arena and keeps the current scores.
  void resetRound();
  /// Clears the scores and starts a fresh match.
  void resetMatch();

  void update(float dt, const std::array<PlayerInput, kPlayerCount>& inputs);

  Tile tileAt(int x, int y) const;
  bool inBounds(int x, int y) const;

  const std::array<Player, kPlayerCount>& players() const { return m_players; }
  const std::vector<Bomb>& bombs() const { return m_bombs; }
  const std::vector<Flame>& flames() const { return m_flames; }
  const std::vector<PowerUp>& powerUps() const { return m_powerUps; }
  const std::array<int, kPlayerCount>& scores() const { return m_scores; }

  RoundState state() const { return m_state; }
  int roundWinner() const { return m_winner; }  // -1 == draw / not decided
  bool matchOver() const { return m_matchOver; }
  int matchWinner() const { return m_matchWinner; }
  int roundNumber() const { return m_round; }
  int roundsToWin() const { return m_rules.roundsToWin; }

  static TileCoord spawnTile(int playerIndex);
  /// The two open tiles leading from a spawn toward the middle of the arena. The
  /// first tile is where a player steps after dropping the opening bomb, the
  /// second is clear of that bomb's one tile blast.
  static std::array<TileCoord, 2> escapeLane(int playerIndex);

 private:
  void generateMap();
  void updatePlayer(int index, const PlayerInput& input, float dt);
  void tryPlaceBomb(int index);
  void detonateExpiredBombs();
  void updateFlames(float dt);
  void applyFlameDamage();
  void collectPowerUps(Player& player);
  void checkRoundEnd();
  void destroyBrick(TileCoord tile);
  void addFlame(TileCoord tile, int owner, bool horizontal, bool center);

  bool collides(const Vec2& center, int playerIndex) const;
  bool isSolidForPlayer(TileCoord tile, int playerIndex) const;
  void updateBombOccupancy();
  bool bombAt(TileCoord tile, int* bombIndex = nullptr) const;
  TileCoord centerTile(const Vec2& pos) const;

  std::mt19937 m_rng;
  MatchRules m_rules;
  std::array<Tile, kCols * kRows> m_tiles{};
  std::array<Player, kPlayerCount> m_players{};
  std::vector<Bomb> m_bombs;
  std::vector<Flame> m_flames;
  std::vector<PowerUp> m_powerUps;
  std::array<int, kPlayerCount> m_scores{};
  std::array<bool, kPlayerCount> m_bombHeld{};
  RoundState m_state = RoundState::Playing;
  int m_winner = -1;
  bool m_matchOver = false;
  int m_matchWinner = -1;
  int m_round = 0;
};

}  // namespace bb
