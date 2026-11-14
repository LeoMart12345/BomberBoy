#include "selftest.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>

#include "game.hpp"

namespace bb {
namespace {

constexpr float kStep = 1.f / 60.f;
constexpr int kFramesPerSecond = 60;

struct TestReport {
  int passed = 0;
  int failed = 0;

  void check(bool condition, const std::string& name) {
    if (condition) {
      ++passed;
      std::cout << "  [PASS] " << name << '\n';
    } else {
      ++failed;
      std::cout << "  [FAIL] " << name << '\n';
    }
  }
};

std::array<PlayerInput, kPlayerCount> idleInputs() { return {}; }

void step(Game& game, int frames, const std::array<PlayerInput, kPlayerCount>& inputs) {
  for (int frame = 0; frame < frames; ++frame) {
    game.update(kStep, inputs);
  }
}

std::array<PlayerInput, kPlayerCount> onlyP1(PlayerInput p1) {
  std::array<PlayerInput, kPlayerCount> result{};
  result[0] = p1;
  return result;
}

void pressOnce(Game& game, int playerIndex) {
  auto inputs = idleInputs();
  inputs[static_cast<std::size_t>(playerIndex)].dropBomb = true;
  game.update(kStep, inputs);
  game.update(kStep, idleInputs());
}

bool centreTileIsFloor(const Game& game, int playerIndex) {
  const Player& player = game.players()[static_cast<std::size_t>(playerIndex)];
  const int x = static_cast<int>(std::floor(player.pos.x));
  const int y = static_cast<int>(std::floor(player.pos.y));
  return game.tileAt(x, y) == Tile::Floor;
}

bool footprintIsClear(const Game& game, int playerIndex) {
  const Player& player = game.players()[static_cast<std::size_t>(playerIndex)];
  const float half = Game::kPlayerHalfSize;

  const int x0 = static_cast<int>(std::floor(player.pos.x - half + 1e-4f));
  const int x1 = static_cast<int>(std::floor(player.pos.x + half - 1e-4f));
  const int y0 = static_cast<int>(std::floor(player.pos.y - half + 1e-4f));
  const int y1 = static_cast<int>(std::floor(player.pos.y + half - 1e-4f));

  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      if (game.tileAt(x, y) != Tile::Floor) {
        return false;
      }
    }
  }
  return true;
}

int countBricks(const Game& game) {
  int bricks = 0;
  for (int y = 0; y < kRows; ++y) {
    for (int x = 0; x < kCols; ++x) {
      if (game.tileAt(x, y) == Tile::Brick) {
        ++bricks;
      }
    }
  }
  return bricks;
}

/// Drops a bomb under player 0 and waits for it to go off.
bool killPlayerZero(Game& game) {
  pressOnce(game, 0);
  const auto inputs = idleInputs();
  for (int frame = 0; frame < 4 * kFramesPerSecond; ++frame) {
    game.update(kStep, inputs);
    if (game.state() == RoundState::RoundOver) {
      return true;
    }
  }
  return false;
}

void testFreshMatch(TestReport& report) {
  std::cout << "fresh match state\n";
  Game game(2024u);
  const auto& players = game.players();

  report.check(players[0].alive && players[1].alive, "both players start alive");
  report.check(players[0].pos.x == 1.5f && players[0].pos.y == 1.5f,
               "player 1 spawns in the top-left corner");
  report.check(players[1].pos.x == static_cast<float>(kCols) - 1.5f &&
                   players[1].pos.y == static_cast<float>(kRows) - 1.5f,
               "player 2 spawns in the bottom-right corner");
  report.check(players[0].bombCapacity == 1 && players[0].blastRange == 1,
               "players start with one bomb and a one tile blast");
  report.check(game.state() == RoundState::Playing, "round starts in the playing state");
  report.check(game.scores()[0] == 0 && game.scores()[1] == 0, "scores start at zero");
  report.check(game.roundNumber() == 1, "first round is round one");
  report.check(!game.matchOver() && game.matchWinner() == -1, "match is not over");
  report.check(game.bombs().empty() && game.flames().empty(), "arena starts empty");
}

void testArenaLayout(TestReport& report) {
  std::cout << "arena layout\n";
  Game game(11u);

  bool borderIsSolid = true;
  for (int y = 0; y < kRows; ++y) {
    for (int x = 0; x < kCols; ++x) {
      const bool border = x == 0 || y == 0 || x == kCols - 1 || y == kRows - 1;
      if (border && game.tileAt(x, y) != Tile::Wall) {
        borderIsSolid = false;
      }
    }
  }
  report.check(borderIsSolid, "the arena is enclosed by walls");

  bool spawnsAreClear = true;
  for (int player = 0; player < kPlayerCount; ++player) {
    const TileCoord spawn = Game::spawnTile(player);
    const int step = player == 0 ? 1 : -1;

    if (game.tileAt(spawn.x, spawn.y) != Tile::Floor) {
      spawnsAreClear = false;
    }
    // The two tiles leading towards the middle of the arena must be walkable.
    if (game.tileAt(spawn.x + step, spawn.y) != Tile::Floor ||
        game.tileAt(spawn.x, spawn.y + step) != Tile::Floor) {
      spawnsAreClear = false;
    }
    // Bricks are never generated in the 5x5 pocket around a spawn.
    for (int dy = -2; dy <= 2; ++dy) {
      for (int dx = -2; dx <= 2; ++dx) {
        if (game.tileAt(spawn.x + dx, spawn.y + dy) == Tile::Brick) {
          spawnsAreClear = false;
        }
      }
    }
  }
  report.check(spawnsAreClear, "spawn corners have clear escape routes and no nearby bricks");

  report.check(countBricks(game) > 20, "the arena is filled with destructible bricks");

  bool pillarsAreSolid = true;
  for (int y = 2; y < kRows - 1; y += 2) {
    for (int x = 2; x < kCols - 1; x += 2) {
      if (game.tileAt(x, y) != Tile::Wall) {
        pillarsAreSolid = false;
      }
    }
  }
  report.check(pillarsAreSolid, "indestructible pillars are spaced across the arena");
}

void testWallCollision(TestReport& report) {
  std::cout << "walls block movement\n";
  Game game(99u);

  PlayerInput left;
  left.left = true;
  step(game, 2 * kFramesPerSecond, onlyP1(left));
  report.check(game.players()[0].pos.x >= 1.f, "player 1 cannot walk into the left border");

  PlayerInput up;
  up.up = true;
  step(game, 2 * kFramesPerSecond, onlyP1(up));
  report.check(game.players()[0].pos.y >= 1.f, "player 1 cannot walk into the top border");

  PlayerInput down;
  down.down = true;
  step(game, 2 * kFramesPerSecond, onlyP1(down));
  report.check(centreTileIsFloor(game, 0), "player 1 always stands on a floor tile");

  PlayerInput wander;
  wander.right = true;
  wander.down = true;
  bool stayedClear = true;
  for (int frame = 0; frame < 4 * kFramesPerSecond; ++frame) {
    game.update(kStep, onlyP1(wander));
    if (game.players()[0].alive && !footprintIsClear(game, 0)) {
      stayedClear = false;
    }
  }
  report.check(stayedClear, "players never overlap walls or bricks");
}

void testBombPlacement(TestReport& report) {
  std::cout << "bomb placement and fuse\n";
  Game game(5u);

  PlayerInput hold;
  hold.dropBomb = true;
  step(game, 30, onlyP1(hold));
  report.check(game.bombs().size() == 1, "holding the bomb key only drops one bomb");
  report.check(game.bombs().size() == 1 && game.bombs()[0].tile.x == 1 &&
                   game.bombs()[0].tile.y == 1,
               "the bomb is placed on the player's tile");
  report.check(game.players()[0].activeBombs == 1, "the bomb counts against the player's limit");

  step(game, 3 * kFramesPerSecond, idleInputs());
  report.check(game.bombs().empty(), "the bomb detonates after its fuse");
  report.check(!game.players()[0].alive, "standing on your own bomb gets you eliminated");
  report.check(game.state() == RoundState::RoundOver, "the round ends when a player dies");
  report.check(game.roundWinner() == 1, "the surviving player wins the round");
  report.check(game.scores()[1] == 1 && game.scores()[0] == 0, "the win is added to the score");
}

void testOwnBombIsSolid(TestReport& report) {
  std::cout << "dropped bombs become solid\n";
  Game game(31u);

  pressOnce(game, 0);

  PlayerInput right;
  right.right = true;
  step(game, 45, onlyP1(right));
  report.check(game.players()[0].pos.x > 3.3f, "the bomber walks clear of their own bomb");
  report.check(game.players()[0].alive, "the bomber is still alive while testing");

  PlayerInput left;
  left.left = true;
  step(game, 40, onlyP1(left));
  report.check(game.players()[0].pos.x >= 2.3f, "a dropped bomb blocks the way back");
  report.check(game.players()[0].alive, "walking into your own bomb does not hurt");

  step(game, 2 * kFramesPerSecond, idleInputs());
  report.check(!game.players()[0].alive, "the blast still catches the bomber hiding next to it");
}

void testChainReaction(TestReport& report) {
  std::cout << "chain reactions\n";
  MatchRules rules;
  rules.startingBombs = 2;
  Game game(123u, rules);

  auto inputs = idleInputs();
  inputs[0].dropBomb = true;
  game.update(kStep, inputs);
  inputs[0].dropBomb = false;

  PlayerInput right;
  right.right = true;
  for (int frame = 0; frame < 90 && game.players()[0].pos.x < 2.45f; ++frame) {
    game.update(kStep, onlyP1(right));
  }

  inputs[0].dropBomb = true;
  game.update(kStep, inputs);
  inputs = idleInputs();
  game.update(kStep, inputs);

  report.check(game.bombs().size() == 2, "a bomber with two slots can drop two bombs");
  report.check(game.players()[0].activeBombs == 2, "both bombs are tracked");

  // How long the second bomb would have burned had nothing reached it.
  const float secondFuseWhenPlaced = game.bombs()[1].fuse;

  float lastSecondFuse = secondFuseWhenPlaced;
  float elapsed = 0.f;
  while (!game.bombs().empty() && elapsed < 3.f) {
    if (game.bombs().size() == 2) {
      lastSecondFuse = game.bombs()[1].fuse;
    }
    game.update(kStep, inputs);
    elapsed += kStep;
  }

  report.check(game.bombs().empty(), "the chain reaction clears every bomb");
  report.check(lastSecondFuse > 0.f,
               "the second bomb still had fuse left when the first blast reached it");
  report.check(secondFuseWhenPlaced - lastSecondFuse > 1.5f,
               "the second bomb blows up early because of the chain");
  report.check(elapsed < Game::kBombFuse + 0.2f,
               "the chain happens on the first bomb's fuse, not the second's");
}


void testRoundReset(TestReport& report) {
  std::cout << "round resets\n";
  Game game(12u);

  report.check(killPlayerZero(game), "the suicide run ends the round");
  report.check(game.scores()[1] == 1, "player 2 is credited with the round");
  const int previousRound = game.roundNumber();

  game.resetRound();
  report.check(game.state() == RoundState::Playing, "the next round starts immediately");
  report.check(game.players()[0].alive && game.players()[1].alive, "both players respawn");
  report.check(game.bombs().empty() && game.flames().empty() && game.powerUps().empty(),
               "bombs, flames and power-ups are cleared");
  report.check(game.scores()[1] == 1 && game.scores()[0] == 0, "the score is carried over");
  report.check(game.roundNumber() == previousRound + 1, "the round counter advances");
  report.check(game.players()[0].pos.x == 1.5f && game.players()[0].pos.y == 1.5f,
               "players return to their spawn corners");
  report.check(game.players()[0].bombCapacity == 1 && game.players()[0].blastRange == 1 &&
                   game.players()[0].activeBombs == 0,
               "loadouts are reset for the new round");
  report.check(game.tileAt(1, 1) == Tile::Floor, "the spawn is clear on the new map");
}

void testMatchFlow(TestReport& report) {
  std::cout << "match flow\n";
  MatchRules rules;
  rules.roundsToWin = 2;
  Game game(21u, rules);

  report.check(killPlayerZero(game), "player 2 takes the opening round");
  report.check(game.scores()[1] == 1, "the opening round is on the board");
  report.check(!game.matchOver(), "one round win does not end a best-of-three");

  game.resetRound();
  report.check(killPlayerZero(game), "player 2 takes the second round");
  report.check(game.scores()[1] == 2, "the second round is on the board");
  report.check(game.matchOver() && game.matchWinner() == 1,
               "the match ends once a player reaches the target");

  game.resetMatch();
  report.check(!game.matchOver() && game.matchWinner() == -1, "a rematch clears the match result");
  report.check(game.scores()[0] == 0 && game.scores()[1] == 0, "a rematch clears the scores");
  report.check(game.roundNumber() == 1, "a rematch starts at round one");
  report.check(game.state() == RoundState::Playing, "a rematch starts a fresh round");
}

void testRandomPlay(TestReport& report) {
  std::cout << "random play stress test\n";
  bool invariantsOk = true;
  bool sawBrickDestruction = false;
  bool sawPowerUp = false;

  for (std::uint32_t seed = 1; seed <= 8; ++seed) {
    Game game(seed);
    std::mt19937 rng(seed * 2654435761u);
    std::uniform_int_distribution<int> directionDist(0, 3);
    std::uniform_int_distribution<int> holdDist(10, 30);

    int direction = 0;
    int hold = 0;
    int baseline = countBricks(game);
    int destroyed = 0;

    for (int frame = 0; frame < 45 * kFramesPerSecond; ++frame) {
      if (game.state() == RoundState::RoundOver) {
        if (destroyed > 0) {
          break;
        }
        game.resetRound();
        baseline = countBricks(game);
        continue;
      }

      if (hold-- <= 0) {
        direction = directionDist(rng);
        hold = holdDist(rng);
      }

      std::array<PlayerInput, kPlayerCount> inputs{};
      PlayerInput p1;
      p1.up = direction == 0;
      p1.down = direction == 1;
      p1.left = direction == 2;
      p1.right = direction == 3;
      p1.dropBomb = frame % 100 == 0;
      inputs[0] = p1;

      game.update(kStep, inputs);

      const int current = countBricks(game);
      if (current < baseline) {
        destroyed += baseline - current;
        baseline = current;
      }
      if (!game.powerUps().empty()) {
        sawPowerUp = true;
      }

      for (int player = 0; player < kPlayerCount; ++player) {
        if (!game.players()[static_cast<std::size_t>(player)].alive) {
          continue;
        }
        if (!footprintIsClear(game, player) || !centreTileIsFloor(game, player)) {
          invariantsOk = false;
        }
      }
      for (const Bomb& bomb : game.bombs()) {
        if (bomb.fuse > Game::kBombFuse + 1e-3f) {
          invariantsOk = false;
        }
      }
    }

    if (destroyed > 0) {
      sawBrickDestruction = true;
    }
  }

  report.check(invariantsOk, "random play never puts a player inside a wall or brick");
  report.check(sawBrickDestruction, "explosions clear bricks during random play");
  report.check(sawPowerUp, "power-ups appear when bricks are destroyed");
}

}  // namespace

bool runSelfTest() {
  std::cout << "BomberBoy headless self-test\n";

  TestReport report;
  testFreshMatch(report);
  testArenaLayout(report);
  testWallCollision(report);
  testBombPlacement(report);
  testOwnBombIsSolid(report);
  testChainReaction(report);
  testRoundReset(report);
  testMatchFlow(report);
  testRandomPlay(report);

  std::cout << "\n" << report.passed << " checks passed, " << report.failed << " failed\n";
  return report.failed == 0;
}

}  // namespace bb

