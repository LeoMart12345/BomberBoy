#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include <SFML/Graphics.hpp>

#include "game.hpp"
#include "renderer.hpp"
#include "selftest.hpp"

namespace {

bb::PlayerInput readPlayerOne() {
  bb::PlayerInput input;
  input.up = sf::Keyboard::isKeyPressed(sf::Keyboard::W);
  input.down = sf::Keyboard::isKeyPressed(sf::Keyboard::S);
  input.left = sf::Keyboard::isKeyPressed(sf::Keyboard::A);
  input.right = sf::Keyboard::isKeyPressed(sf::Keyboard::D);
  return input;
}

bb::PlayerInput readPlayerTwo() {
  bb::PlayerInput input;
  input.up = sf::Keyboard::isKeyPressed(sf::Keyboard::Up);
  input.down = sf::Keyboard::isKeyPressed(sf::Keyboard::Down);
  input.left = sf::Keyboard::isKeyPressed(sf::Keyboard::Left);
  input.right = sf::Keyboard::isKeyPressed(sf::Keyboard::Right);
  return input;
}

/// Returns the player index a key press drops a bomb for, or -1 for other keys.
int bombKeyPlayer(sf::Keyboard::Key key) {
  if (key == sf::Keyboard::Space) {
    return 0;
  }
  if (key == sf::Keyboard::Enter || key == sf::Keyboard::RControl ||
      key == sf::Keyboard::RShift) {
    return 1;
  }
  return -1;
}

void printUsage() {
  std::cout << "BomberBoy - two player bomb battle\n\n"
            << "Usage: bomberboy [options]\n"
            << "  --selftest     run the headless logic checks and exit\n"
            << "  --seed <n>     build the arena from a fixed seed\n"
            << "  --screenshot <file>  render one frame to a PNG instead of opening a window\n"
            << "  --autoplay <s>       scripted play seconds before the screenshot\n"
            << "  -h, --help     show this help\n\n"
            << "Controls\n"
            << "  Player 1   W A S D to move, Space to drop a bomb\n"
            << "  Player 2   Arrow keys to move, Enter or Right Ctrl to drop a bomb\n"
            << "  R  restart round (or rematch once the match is over)\n"
            << "  P  pause    Esc  quit\n";
}

/// Fixed input script used by --screenshot so the captured frame shows the action:
/// both bombers drop their opening bomb and step into the escape lane.
void playDemo(bb::Game& game, float seconds) {
  const float step = 1.0f / 60.0f;
  int frame = 0;
  for (float elapsed = 0.0f; elapsed < seconds; elapsed += step, ++frame) {
    std::array<bb::PlayerInput, bb::kPlayerCount> inputs{};

    bb::PlayerInput playerOne;
    playerOne.dropBomb = frame >= 8 && frame < 12;
    playerOne.right = frame >= 20 && frame < 62;

    bb::PlayerInput playerTwo;
    playerTwo.dropBomb = frame >= 40 && frame < 44;
    playerTwo.left = frame >= 54 && frame < 96;

    inputs[0] = playerOne;
    inputs[1] = playerTwo;
    game.update(step, inputs);
  }
}

bool saveScreenshot(const std::string& path, bool hasSeed, std::uint32_t seed, float autoplay) {
  sf::RenderTexture target;
  if (!target.create(bb::Renderer::kWindowWidth, bb::Renderer::kWindowHeight)) {
    std::cerr << "error: could not create an offscreen render target\n";
    return false;
  }

  bb::Game game = hasSeed ? bb::Game(seed) : bb::Game();
  if (autoplay > 0.0f) {
    playDemo(game, autoplay);
  }

  bb::Renderer renderer(target);
  renderer.draw(game, false);
  target.display();

  if (!target.getTexture().copyToImage().saveToFile(path)) {
    std::cerr << "error: could not write " << path << "\n";
    return false;
  }

  std::cout << "wrote " << path << "\n";
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  bool runTests = false;
  bool hasSeed = false;
  std::uint32_t seed = 0;
  std::string screenshotPath;
  float autoplay = 0.0f;

  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--selftest" || argument == "--test") {
      runTests = true;
    } else if (argument == "--seed" && i + 1 < argc) {
      seed = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 10));
      hasSeed = true;
    } else if (argument == "--screenshot" && i + 1 < argc) {
      screenshotPath = argv[++i];
    } else if (argument == "--autoplay" && i + 1 < argc) {
      autoplay = std::min(std::max(std::strtof(argv[++i], nullptr), 0.0f), 600.0f);
    } else if (argument == "--help" || argument == "-h") {
      printUsage();
      return 0;
    } else {
      std::cerr << "Unknown option: " << argument << "\n\n";
      printUsage();
      return 1;
    }
  }

  if (runTests) {
    return bb::runSelfTest() ? 0 : 1;
  }

  if (!screenshotPath.empty()) {
    return saveScreenshot(screenshotPath, hasSeed, seed, autoplay) ? 0 : 1;
  }

  sf::RenderWindow window(
      sf::VideoMode(bb::Renderer::kWindowWidth, bb::Renderer::kWindowHeight), "BomberBoy",
      sf::Style::Titlebar | sf::Style::Close);
  window.setFramerateLimit(60);
  window.setKeyRepeatEnabled(false);

  bb::Game game = hasSeed ? bb::Game(seed) : bb::Game();
  bb::Renderer renderer(window);
  if (!renderer.hasFont()) {
    std::cerr << "warning: no system font found, HUD text will be hidden\n";
  }
  if (!renderer.hasSprites()) {
    std::cerr << "warning: sprite art not found (looked for assets/sprites), using shape "
                 "fallback\n";
  }

  sf::Clock frameClock;
  bool paused = false;
  // Bomb drops are edge triggered from key events: a tap can never be missed by
  // the polling that drives movement, and a held key still only drops one bomb.
  std::array<bool, bb::kPlayerCount> bombPressed{};

  while (window.isOpen()) {
    sf::Event event;
    while (window.pollEvent(event)) {
      if (event.type == sf::Event::Closed) {
        window.close();
      } else if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Escape) {
          window.close();
        } else if (event.key.code == sf::Keyboard::R) {
          if (game.matchOver()) {
            game.resetMatch();
          } else {
            game.resetRound();
          }
          paused = false;
          bombPressed = {};
        } else if (event.key.code == sf::Keyboard::P) {
          paused = !paused;
          bombPressed = {};
        } else {
          const int who = bombKeyPlayer(event.key.code);
          if (who >= 0) {
            bombPressed[static_cast<std::size_t>(who)] = true;
          }
        }
      }
    }

    const float dt = std::min(frameClock.restart().asSeconds(), 0.05f);
    if (!paused) {
      std::array<bb::PlayerInput, bb::kPlayerCount> inputs{readPlayerOne(), readPlayerTwo()};
      for (std::size_t i = 0; i < inputs.size(); ++i) {
        inputs[i].dropBomb = bombPressed[i];
      }
      game.update(dt, inputs);
    }
    bombPressed = {};

    renderer.draw(game, paused);
    window.display();
  }

  return 0;
}
