#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include <SFML/Graphics.hpp>

#include "ai.hpp"
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
            << "  --cpu          let the computer play player two\n"
            << "  --fps <n>      frame cap, 0 runs uncapped (default 60)\n"
            << "  --scale <n>    start with an n times bigger window (1-4)\n"
            << "  --fullscreen   start in fullscreen\n"
            << "  --screenshot <file>  render one frame to a PNG instead of opening a window\n"
            << "  --autoplay <s>       scripted play seconds before the screenshot\n"
            << "  -h, --help     show this help\n\n"
            << "Controls\n"
            << "  Player 1   W A S D to move, Space to drop a bomb\n"
            << "  Player 2   Arrow keys to move, Enter or Right Ctrl to drop a bomb\n"
            << "  C  hand player two to the computer (or take it back)\n"
            << "  R  restart round (or rematch once the match is over)\n"
            << "  P  pause    F1  help    F2  screenshot    F3  FPS counter\n"
            << "  F11 fullscreen    Esc  quit\n";
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
  bb::UiState ui;
  ui.showFps = false;  // clean capture for the README
  renderer.draw(game, ui);
  target.display();

  if (!target.getTexture().copyToImage().saveToFile(path)) {
    std::cerr << "error: could not write " << path << "\n";
    return false;
  }

  std::cout << "wrote " << path << "\n";
  return true;
}

/// Grabs what the window currently shows and writes it to the next free
/// `bomberboy-shot-<n>.png`. Must run after the frame was drawn and before it is
/// displayed, which is when the window still holds the finished frame.
bool saveWindowShot(sf::RenderWindow& window, int& shotNumber) {
  sf::Texture frame;
  if (!frame.create(window.getSize().x, window.getSize().y)) {
    std::cerr << "error: could not create a texture for the screenshot\n";
    return false;
  }
  frame.update(window);
  const sf::Image image = frame.copyToImage();

  std::string path;
  do {
    path = "bomberboy-shot-" + std::to_string(++shotNumber) + ".png";
  } while (std::ifstream(path).good());

  if (!image.saveToFile(path)) {
    std::cerr << "error: could not write " << path << "\n";
    return false;
  }
  std::cout << "wrote " << path << " (" << image.getSize().x << "x" << image.getSize().y << ")\n";
  return true;
}

/// Score and round in the title bar, so the match stays readable while the
/// window is behind another one.
std::string makeWindowTitle(const bb::Game& game, bool cpuOpponent) {
  const std::string second = cpuOpponent ? "CPU" : "P2";
  const auto& scores = game.scores();
  std::string title = "BomberBoy  -  P1 " + std::to_string(scores[0]) + " : " +
                      std::to_string(scores[1]) + " " + second;

  if (game.matchOver()) {
    if (game.matchWinner() == 0) {
      title += "  -  P1 wins the match";
    } else {
      title += "  -  " + second + " wins the match";
    }
  } else {
    title += "  -  Round " + std::to_string(game.roundNumber());
  }
  return title;
}

}  // namespace

int main(int argc, char** argv) {
  bool runTests = false;
  bool hasSeed = false;
  bool cpuOpponent = false;
  bool fullscreen = false;
  std::uint32_t seed = 0;
  std::string screenshotPath;
  float autoplay = 0.0f;
  int scale = 1;
  int fpsLimit = 60;

  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--selftest" || argument == "--test") {
      runTests = true;
    } else if (argument == "--cpu") {
      cpuOpponent = true;
    } else if (argument == "--fullscreen") {
      fullscreen = true;
    } else if (argument == "--seed" && i + 1 < argc) {
      seed = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 10));
      hasSeed = true;
    } else if (argument == "--fps" && i + 1 < argc) {
      fpsLimit = std::min(std::max(std::atoi(argv[++i]), 0), 1000);
    } else if (argument == "--scale" && i + 1 < argc) {
      scale = std::min(std::max(std::atoi(argv[++i]), 1), 4);
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

  const sf::VideoMode windowedMode(bb::Renderer::kWindowWidth * static_cast<unsigned>(scale),
                                   bb::Renderer::kWindowHeight * static_cast<unsigned>(scale));

  sf::RenderWindow window;
  window.create(fullscreen ? sf::VideoMode::getDesktopMode() : windowedMode, "BomberBoy",
                fullscreen ? sf::Style::Fullscreen : sf::Style::Default);
  window.setFramerateLimit(static_cast<unsigned>(fpsLimit));
  window.setKeyRepeatEnabled(false);
  window.setView(bb::Renderer::viewFor(window.getSize()));

  bb::Game game = hasSeed ? bb::Game(seed) : bb::Game();
  bb::AiBrain brain(1);
  bb::Renderer renderer(window);
  if (!renderer.hasFont()) {
    std::cerr << "warning: no system font found, HUD text will be hidden\n";
  }
  if (!renderer.hasSprites()) {
    std::cerr << "warning: sprite art not found (looked for assets/sprites), using shape "
                 "fallback\n";
  }

  // Switching to or from fullscreen re-creates the window, and with it the
  // OpenGL context, so the font and the sprite textures have to be reloaded.
  const auto applyWindowMode = [&](bool wantFullscreen) {
    const sf::VideoMode mode = wantFullscreen ? sf::VideoMode::getDesktopMode() : windowedMode;
    window.create(mode, "BomberBoy", wantFullscreen ? sf::Style::Fullscreen : sf::Style::Default);
    window.setFramerateLimit(static_cast<unsigned>(fpsLimit));
    window.setKeyRepeatEnabled(false);
    window.setView(bb::Renderer::viewFor(window.getSize()));
    renderer.reloadAssets();
  };

  sf::Clock frameClock;
  bool paused = false;
  bool showHelp = false;
  bool showFps = true;
  bool shotRequested = false;
  int shotNumber = 0;
  float fpsAverage = 0.f;
  float frameMillis = 0.f;
  std::string windowTitle;
  // Bomb drops are edge triggered from key events: a tap can never be missed by
  // the polling that drives movement, and a held key still only drops one bomb.
  std::array<bool, bb::kPlayerCount> bombPressed{};

  while (window.isOpen()) {
    sf::Event event;
    while (window.pollEvent(event)) {
      if (event.type == sf::Event::Closed) {
        window.close();
      } else if (event.type == sf::Event::Resized) {
        window.setView(bb::Renderer::viewFor(window.getSize()));
      } else if (event.type == sf::Event::LostFocus) {
        // Do not let a round run on while the player is in another window.
        if (game.state() == bb::RoundState::Playing) {
          paused = true;
        }
        bombPressed = {};
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
          brain.reset();
        } else if (event.key.code == sf::Keyboard::P) {
          paused = !paused;
          bombPressed = {};
        } else if (event.key.code == sf::Keyboard::C) {
          cpuOpponent = !cpuOpponent;
          bombPressed = {};
          brain.reset();
        } else if (event.key.code == sf::Keyboard::F1) {
          showHelp = !showHelp;
          bombPressed = {};
        } else if (event.key.code == sf::Keyboard::F2) {
          shotRequested = true;
        } else if (event.key.code == sf::Keyboard::F3) {
          showFps = !showFps;
        } else if (event.key.code == sf::Keyboard::F11) {
          fullscreen = !fullscreen;
          applyWindowMode(fullscreen);
          bombPressed = {};
        } else {
          const int who = bombKeyPlayer(event.key.code);
          if (who >= 0) {
            bombPressed[static_cast<std::size_t>(who)] = true;
          }
        }
      }
    }

    // The FPS readout follows the real frame time; the simulation keeps its cap
    // so a slow frame cannot teleport anyone through a wall.
    const float rawDt = frameClock.restart().asSeconds();
    const float dt = std::min(rawDt, 0.05f);
    if (rawDt > 0.f) {
      const float instantFps = 1.f / rawDt;
      const float instantMillis = rawDt * 1000.f;
      const float smoothing = 0.1f;
      fpsAverage =
          fpsAverage > 0.f ? fpsAverage + (instantFps - fpsAverage) * smoothing : instantFps;
      frameMillis =
          frameMillis > 0.f ? frameMillis + (instantMillis - frameMillis) * smoothing : instantMillis;
    }

    // The help panel freezes the match; P keeps its own pause state.
    if (!paused && !showHelp) {
      std::array<bb::PlayerInput, bb::kPlayerCount> inputs{};
      inputs[0] = readPlayerOne();
      inputs[0].dropBomb = bombPressed[0];
      if (cpuOpponent) {
        inputs[1] = brain.think(game, dt);
      } else {
        inputs[1] = readPlayerTwo();
        inputs[1].dropBomb = bombPressed[1];
      }
      game.update(dt, inputs);
    }
    bombPressed = {};

    const std::string title = makeWindowTitle(game, cpuOpponent);
    if (title != windowTitle) {
      window.setTitle(title);
      windowTitle = title;
    }

    bb::UiState ui;
    ui.paused = paused && !showHelp;
    ui.showHelp = showHelp;
    ui.showFps = showFps;
    ui.cpuPlayer = cpuOpponent ? 1 : -1;
    ui.framesPerSecond = fpsAverage;
    ui.frameMilliseconds = frameMillis;

    renderer.draw(game, ui);
    if (shotRequested) {
      shotRequested = false;
      saveWindowShot(window, shotNumber);
    }
    window.display();
  }

  return 0;
}
