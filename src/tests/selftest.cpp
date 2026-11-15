#include "tests/selftest.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "cli/cli.hpp"
#include "core/ai.hpp"
#include "core/game.hpp"
#include "render/assets.hpp"
#include "render/renderer.hpp"

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

bool nearValue(float a, float b) { return std::fabs(a - b) < 0.01f; }

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

/// The constructor clamps the rules it is handed, so a hand-written `MatchRules`
/// can never produce a loadout the game cannot represent.
void testMatchRules(TestReport& report) {
  std::cout << "match rules\n";

  MatchRules extreme;
  extreme.startingBombs = 99;
  extreme.startingRange = 0;
  extreme.startingSpeed = 100.f;
  extreme.roundsToWin = 0;
  const Game clamped(4u, extreme);
  report.check(clamped.players()[0].bombCapacity == 8,
               "the starting bomb count is capped at eight");
  report.check(clamped.players()[0].blastRange == 1, "the blast range never drops below one");
  report.check(nearValue(clamped.players()[0].moveSpeed, Game::kMaxMoveSpeed),
               "the starting speed is capped at the maximum");
  report.check(clamped.roundsToWin() == 1, "a match always needs at least one round win");

  MatchRules tuned;
  tuned.startingBombs = 3;
  tuned.startingRange = 4;
  tuned.startingSpeed = 1.5f;
  tuned.roundsToWin = 5;
  const Game custom(5u, tuned);
  report.check(custom.players()[0].bombCapacity == 3 && custom.players()[0].blastRange == 4 &&
                   nearValue(custom.players()[0].moveSpeed, 1.5f) && custom.roundsToWin() == 5,
               "rules inside the valid range are used as they are");
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
  bool spawnsAreBoxedIn = true;
  for (int player = 0; player < kPlayerCount; ++player) {
    const TileCoord spawn = Game::spawnTile(player);
    const auto lane = Game::escapeLane(player);

    if (game.tileAt(spawn.x, spawn.y) != Tile::Floor) {
      spawnsAreClear = false;
    }
    // The escape lane towards the middle of the arena must be walkable so a
    // player can step out of their own bomb's blast.
    if (game.tileAt(lane[0].x, lane[0].y) != Tile::Floor ||
        game.tileAt(lane[1].x, lane[1].y) != Tile::Floor) {
      spawnsAreClear = false;
    }

    int neighbouringCrates = 0;
    const int steps[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    for (const auto& step : steps) {
      if (game.tileAt(spawn.x + step[0], spawn.y + step[1]) == Tile::Brick) {
        ++neighbouringCrates;
      }
    }
    if (neighbouringCrates < 1) {
      spawnsAreBoxedIn = false;
    }
  }
  report.check(spawnsAreClear, "spawn tiles have a clear two tile escape lane");
  report.check(spawnsAreBoxedIn, "every spawn starts next to a breakable crate");

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

void testOpeningBomb(TestReport& report) {
  std::cout << "opening bomb breaks crates\n";
  bool everySpawnHasATarget = true;
  bool escapeLaneIsSafe = true;
  bool cratesWentAway = true;

  for (std::uint32_t seed = 1; seed <= 8; ++seed) {
    for (int player = 0; player < kPlayerCount; ++player) {
      Game game(seed);
      const TileCoord spawn = Game::spawnTile(player);
      const auto lane = Game::escapeLane(player);

      // The bomb must have something to break right next to the spawn.
      int cratesBefore = 0;
      const int steps[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
      for (const auto& step : steps) {
        if (game.tileAt(spawn.x + step[0], spawn.y + step[1]) == Tile::Brick) {
          ++cratesBefore;
        }
      }
      if (cratesBefore < 1) {
        everySpawnHasATarget = false;
      }

      pressOnce(game, player);

      // Run down the escape lane, away from the bomb.
      PlayerInput run;
      run.right = lane[0].x > spawn.x;
      run.left = lane[0].x < spawn.x;
      run.down = lane[0].y > spawn.y;
      run.up = lane[0].y < spawn.y;
      auto inputs = idleInputs();
      inputs[static_cast<std::size_t>(player)] = run;
      step(game, 45, inputs);
      step(game, 3 * kFramesPerSecond, idleInputs());

      if (!game.players()[static_cast<std::size_t>(player)].alive) {
        escapeLaneIsSafe = false;
      }

      int cratesAfter = 0;
      for (const auto& step : steps) {
        if (game.tileAt(spawn.x + step[0], spawn.y + step[1]) == Tile::Brick) {
          ++cratesAfter;
        }
      }
      if (cratesAfter >= cratesBefore) {
        cratesWentAway = false;
      }
    }
  }

  report.check(everySpawnHasATarget, "the opening bomb always has a crate in range");
  report.check(escapeLaneIsSafe, "stepping into the escape lane survives the opening bomb");
  report.check(cratesWentAway, "the opening bomb destroys a crate next to the spawn");
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

void testSpriteArt(TestReport& report) {
  std::cout << "sprite art on disk\n";

  const std::string directory = SpriteSet::locateDirectory();
  if (directory.empty()) {
    std::cout << "  [SKIP] sprite art is not installed, the renderer uses its shape "
                 "fallback\n";
    return;
  }

  const SpriteAudit audit = SpriteSet::audit(directory);
  report.check(audit.missing.empty(),
               std::to_string(audit.expected) + " sprites are present in " + directory);
  report.check(audit.malformed.empty(), "every sprite in " + directory + " is a square PNG");

  for (const std::string& name : audit.missing) {
    std::cout << "         missing " << name << '\n';
  }
  for (const std::string& problem : audit.malformed) {
    std::cout << "         " << problem << '\n';
  }
}


/// The parts of a view the layout checks care about, as a rect.
sf::FloatRect viewRect(const sf::Vector2u& windowSize) {
  const sf::View view = Renderer::viewFor(windowSize);
  const sf::Vector2f size = view.getSize();
  const sf::Vector2f center = view.getCenter();
  return sf::FloatRect({center.x - size.x * 0.5f, center.y - size.y * 0.5f}, {size.x, size.y});
}

/// The window can be resized and switched to fullscreen, so the mapping from the
/// fixed 720 x 718 canvas onto the window has to keep the arena centred and the
/// art crisp. This is plain math and runs without an OpenGL context.
void testLayoutView(TestReport& report) {
  std::cout << "window scaling\n";

  const float canvasWidth = static_cast<float>(Renderer::kWindowWidth);
  const float canvasHeight = static_cast<float>(Renderer::kWindowHeight);
  const auto width = Renderer::kWindowWidth;
  const auto height = Renderer::kWindowHeight;

  const sf::FloatRect native = viewRect(sf::Vector2u{width, height});
  report.check(nearValue(native.size.x, canvasWidth) && nearValue(native.size.y, canvasHeight) &&
                   nearValue(native.position.x, 0.f) && nearValue(native.position.y, 0.f),
               "a 720x718 window shows exactly the canvas");

  const sf::FloatRect doubled = viewRect(sf::Vector2u{width * 2, height * 2});
  report.check(nearValue(doubled.size.x, canvasWidth) && nearValue(doubled.size.y, canvasHeight),
               "a 2x window zooms the canvas instead of showing more of it");

  const sf::FloatRect odd = viewRect(sf::Vector2u{1000, 800});
  report.check(nearValue(odd.size.x, 1000.f) && nearValue(odd.size.y, 800.f) &&
                   nearValue(odd.position.x, -140.f) && nearValue(odd.position.y, -41.f),
               "an odd window size keeps the canvas centred with bars around it");

  const sf::FloatRect small = viewRect(sf::Vector2u{360, 359});
  report.check(nearValue(small.size.x, canvasWidth) && nearValue(small.size.y, canvasHeight) &&
                   nearValue(small.position.x, 0.f) && nearValue(small.position.y, 0.f),
               "a window smaller than the canvas still shows all of it");

  const sf::FloatRect tall = viewRect(sf::Vector2u{width, height * 2});
  report.check(nearValue(tall.position.x, 0.f) &&
                   nearValue(tall.position.y, -canvasHeight * 0.5f) &&
                   nearValue(tall.size.x, canvasWidth) && nearValue(tall.size.y, height * 2.f),
               "a tall window letterboxes the canvas vertically");

  const sf::FloatRect degenerate = viewRect(sf::Vector2u{0, 0});
  report.check(degenerate.size.x > 0.f && degenerate.size.y > 0.f,
               "a zero sized window never asks for a degenerate view");
}

bool sameInput(const PlayerInput& a, const PlayerInput& b) {
  return a.up == b.up && a.down == b.down && a.left == b.left && a.right == b.right &&
         a.dropBomb == b.dropBomb;
}

void testAiOpponent(TestReport& report) {
  std::cout << "computer opponent\n";

  bool droppedBomb = false;
  bool clearedCrate = false;
  bool died = false;

  for (const std::uint32_t seed : {3u, 7u, 21u}) {
    Game game(seed);
    AiBrain brain(1);
    int bricks = countBricks(game);

    // Player one never moves or bombs here, so anything the brain walks into is
    // its own bomb.
    for (int frame = 0; frame < 40 * kFramesPerSecond; ++frame) {
      if (game.state() == RoundState::RoundOver) {
        game.resetRound();
        brain.reset();
        bricks = countBricks(game);
        continue;
      }

      auto inputs = idleInputs();
      inputs[1] = brain.think(game, kStep);
      game.update(kStep, inputs);

      if (!game.bombs().empty()) {
        droppedBomb = true;
      }
      if (!game.players()[1].alive) {
        died = true;
      }

      const int current = countBricks(game);
      if (current < bricks) {
        clearedCrate = true;
      }
      bricks = current;
    }
  }

  report.check(droppedBomb, "the computer opponent drops bombs");
  report.check(clearedCrate, "the computer opponent clears crates");
  report.check(!died, "the computer opponent never blows itself up");

  bool deterministic = true;
  Game first(11u);
  Game second(11u);
  AiBrain firstBrain(1);
  AiBrain secondBrain(1);
  for (int frame = 0; frame < 20 * kFramesPerSecond && deterministic; ++frame) {
    const PlayerInput firstInput = firstBrain.think(first, kStep);
    const PlayerInput secondInput = secondBrain.think(second, kStep);
    if (!sameInput(firstInput, secondInput)) {
      deterministic = false;
    }

    auto firstInputs = idleInputs();
    firstInputs[1] = firstInput;
    first.update(kStep, firstInputs);

    auto secondInputs = idleInputs();
    secondInputs[1] = secondInput;
    second.update(kStep, secondInputs);
  }
  report.check(deterministic, "two brains fed the same match play it the same way");
}

/// The parser lives in `cli.cpp` so the checks can hand it the same strings a
/// shell would, without a window or an OpenGL context.
void testCommandLine(TestReport& report) {
  std::cout << "command line\n";

  const auto parse = [](const std::vector<std::string>& arguments) {
    return parseOptions(arguments);
  };

  const ParsedOptions defaults = parse({});
  report.check(defaults.ok && !defaults.wantsHelp && !defaults.options.runSelfTest &&
                   !defaults.options.cpuOpponent && !defaults.options.fullscreen &&
                   !defaults.options.hasSeed && defaults.options.screenshotPath.empty() &&
                   defaults.options.scale == 1 && defaults.options.frameCap == 60 &&
                   nearValue(defaults.options.autoplaySeconds, 0.f),
               "an empty command line gives the documented defaults");

  const ParsedOptions all = parse({"--cpu", "--seed", "42", "--fps", "0", "--scale", "2",
                                   "--fullscreen", "--autoplay", "2.5", "--screenshot",
                                   "shot.png"});
  report.check(all.ok && all.options.cpuOpponent && all.options.fullscreen &&
                   all.options.hasSeed && all.options.seed == 42 && all.options.frameCap == 0 &&
                   all.options.scale == 2 && nearValue(all.options.autoplaySeconds, 2.5f) &&
                   all.options.screenshotPath == "shot.png",
               "every option can be combined in any order");

  report.check(parse({"--help"}).wantsHelp && parse({"-h"}).wantsHelp,
               "both spellings of the help flag ask for the usage text");
  report.check(parse({"--test"}).options.runSelfTest, "--test is an alias for --selftest");

  const ParsedOptions unknown = parse({"--nope"});
  report.check(!unknown.ok && unknown.error.find("--nope") != std::string::npos,
               "an unknown option is an error that names it");
  report.check(!parse({"--fps"}).ok, "an option that needs a value fails without one");
  report.check(!parse({"--screenshot"}).ok && !parse({"--screenshot", ""}).ok,
               "--screenshot needs a file name");
  report.check(!parse({"--fps", "fast"}).ok && !parse({"--fps", "-1"}).ok &&
                   !parse({"--fps", "12x"}).ok &&
                   !parse({"--fps", std::to_string(kMaxFrameCap + 1)}).ok,
               "--fps rejects text, negative numbers, trailing junk and too large caps");
  report.check(!parse({"--scale", "0"}).ok && !parse({"--scale", "9"}).ok,
               "--scale only accepts the documented 1 to 4");
  report.check(!parse({"--seed", "-3"}).ok && !parse({"--seed", "4294967296"}).ok,
               "--seed rejects negative numbers and values that do not fit 32 bits");
  report.check(!parse({"--autoplay", "soon"}).ok, "--autoplay needs a number of seconds");

  const std::string usage = usageText();
  bool everyOptionListed = true;
  for (const char* flag : {"--selftest", "--seed", "--cpu", "--fps", "--scale", "--fullscreen",
                           "--screenshot", "--autoplay", "--help"}) {
    everyOptionListed = everyOptionListed && usage.find(flag) != std::string::npos;
  }
  report.check(everyOptionListed, "the usage text lists every option");
}

}  // namespace

bool runSelfTest() {
  std::cout << "BomberBoy headless self-test\n";

  TestReport report;
  testFreshMatch(report);
  testMatchRules(report);
  testArenaLayout(report);
  testWallCollision(report);
  testBombPlacement(report);
  testOpeningBomb(report);
  testOwnBombIsSolid(report);
  testChainReaction(report);
  testRoundReset(report);
  testMatchFlow(report);
  testRandomPlay(report);
  testAiOpponent(report);
  testLayoutView(report);
  testCommandLine(report);
  testSpriteArt(report);

  std::cout << "\n" << report.passed << " checks passed, " << report.failed << " failed\n";
  return report.failed == 0;
}

}  // namespace bb

