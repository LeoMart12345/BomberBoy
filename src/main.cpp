#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

#include <SFML/Graphics.hpp>

#include "ai.hpp"
#include "cli.hpp"
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

/// Largest `--scale` that still fits on the desktop, so a big scale never opens a
/// window that is wider than the screen. The renderer letterboxes whatever is
/// left over, so falling back here only costs a little sharpness.
int scaleForDesktop(int requested, const sf::VideoMode& desktop) {
  for (int scale = requested; scale > 1; --scale) {
    const unsigned width = bb::Renderer::kWindowWidth * static_cast<unsigned>(scale);
    const unsigned height = bb::Renderer::kWindowHeight * static_cast<unsigned>(scale);
    if (width <= desktop.width && height <= desktop.height) {
      return scale;
    }
  }
  return 1;
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
  const bb::ParsedOptions parsed = bb::parseOptions(argc, argv);
  if (!parsed.ok) {
    std::cerr << "bomberboy: " << parsed.error << "\n\n" << bb::usageText();
    return 1;
  }
  if (parsed.wantsHelp) {
    std::cout << bb::usageText();
    return 0;
  }

  const bb::Options& options = parsed.options;
  if (options.runSelfTest) {
    return bb::runSelfTest() ? 0 : 1;
  }
  if (!options.screenshotPath.empty()) {
    return saveScreenshot(options.screenshotPath, options.hasSeed, options.seed,
                          options.autoplaySeconds)
               ? 0
               : 1;
  }

  const sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
  const int scale = scaleForDesktop(options.scale, desktop);
  // Fullscreen ignores the windowed size, but the scale still decides how big the
  // window is once F11 brings it back, so only mention the clamp for windowed runs.
  if (!options.fullscreen && scale != options.scale) {
    std::cout << "note: the desktop fits " << scale << "x, not " << options.scale
              << "x - the window was scaled down and the canvas is letterboxed\n";
  }

  const sf::VideoMode windowedMode(bb::Renderer::kWindowWidth * static_cast<unsigned>(scale),
                                   bb::Renderer::kWindowHeight * static_cast<unsigned>(scale));

  sf::RenderWindow window;
  window.create(options.fullscreen ? desktop : windowedMode, "BomberBoy",
                options.fullscreen ? sf::Style::Fullscreen : sf::Style::Default);

  // Start in the middle of the desktop instead of wherever the window manager
  // feels like - a scaled-up window can otherwise open with its HUD off-screen.
  const sf::Vector2i centred(
      std::max(0, (static_cast<int>(desktop.width) - static_cast<int>(windowedMode.width)) / 2),
      std::max(0, (static_cast<int>(desktop.height) - static_cast<int>(windowedMode.height)) / 2));

  if (!options.fullscreen) {
    window.setPosition(centred);
  }
  window.setFramerateLimit(static_cast<unsigned>(options.frameCap));
  window.setKeyRepeatEnabled(false);
  window.setView(bb::Renderer::viewFor(window.getSize()));

  bb::Game game = options.hasSeed ? bb::Game(options.seed) : bb::Game();
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
    window.setFramerateLimit(static_cast<unsigned>(options.frameCap));
    window.setKeyRepeatEnabled(false);
    window.setView(bb::Renderer::viewFor(window.getSize()));
    if (!wantFullscreen) {
      window.setPosition(centred);
    }
    renderer.reloadAssets();
  };

  sf::Clock frameClock;
  bool cpuOpponent = options.cpuOpponent;
  bool fullscreen = options.fullscreen;
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
          // Closing the panel with Esc is what everyone tries first; quitting by
          // accident while reading the controls would be a nasty surprise.
          if (showHelp) {
            showHelp = false;
          } else {
            window.close();
          }
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
