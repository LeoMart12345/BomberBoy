#pragma once

#include <SFML/Graphics.hpp>

#include <string>

#include "assets.hpp"
#include "game.hpp"

namespace bb {

/// Everything the drawing code needs that is not part of the match itself: the
/// panels that are open and the frame timings behind the FPS counter.
struct UiState {
  bool paused = false;
  bool showHelp = false;
  bool showFps = true;
  /// Index of the player the computer drives, or -1 when two humans play.
  int cpuPlayer = -1;
  float framesPerSecond = 0.f;
  float frameMilliseconds = 0.f;
};

/// Draws a Game with the pixel-art sprites from `assets/sprites`, falling back to
/// plain SFML shapes when the art files are missing.
///
/// Everything is drawn in a fixed 720 x 718 logical canvas; `viewFor()` maps that
/// canvas onto whatever window the player resized to.
class Renderer {
 public:
  static constexpr float kTileSize = 48.f;
  static constexpr unsigned kHudHeight = 64;
  static constexpr unsigned kFooterHeight = 30;
  static constexpr unsigned kWindowWidth = static_cast<unsigned>(kCols * kTileSize);
  static constexpr unsigned kWindowHeight =
      static_cast<unsigned>(kRows * kTileSize) + kHudHeight + kFooterHeight;

  explicit Renderer(sf::RenderTarget& target);

  void draw(const Game& game, const UiState& ui);

  /// View that letterboxes the logical canvas into `windowSize`. The zoom snaps
  /// to whole numbers whenever that keeps at least 85% of the available space,
  /// so the pixel art stays crisp on any window size, including fullscreen.
  static sf::View viewFor(const sf::Vector2u& windowSize);

  /// Reloads the font and the sprites. Needed after the window - and with it the
  /// OpenGL context - has been recreated by a fullscreen toggle.
  void reloadAssets();

  /// False when no usable system font was found (text is then skipped).
  bool hasFont() const { return m_hasFont; }
  /// False when the sprite set is missing and the shape fallback is in use.
  bool hasSprites() const { return m_sprites.ready(); }
  const std::string& spriteDirectory() const { return m_sprites.directory(); }

 private:
  void loadFont();

  void drawFloor();
  void drawTiles(const Game& game);
  void drawPowerUps(const Game& game);
  void drawBombs(const Game& game);
  void drawFlames(const Game& game);
  void drawPlayers(const Game& game);
  void drawBombMarkers(const Game& game);
  void drawPlayer(const Player& player, int index);
  void drawHud(const Game& game, const UiState& ui);
  void drawPlayerBlock(const Game& game, int index, bool rightAligned, bool computer);
  void drawFooter(const UiState& ui);
  void drawOverlays(const Game& game, const UiState& ui);
  void drawHelpPanel();

  void drawText(const std::string& text, unsigned size, float x, float y, sf::Color color,
                bool centered = true, bool bold = true);
  /// Left aligned at `x`, vertically centred on `centerY`.
  void drawTextLeft(const std::string& text, unsigned size, float x, float centerY,
                    sf::Color color);
  /// Right aligned so the text ends at `x`, vertically centred on `centerY`.
  void drawTextRight(const std::string& text, unsigned size, float x, float centerY,
                     sf::Color color);
  float textWidth(const std::string& text, unsigned size, bool bold = true);
  void drawPip(float x, float y, sf::Color color, bool filled);
  /// The power-up sprite for `kind`, or a coloured badge when the art is missing.
  void drawStatIcon(PowerUpKind kind, const sf::Vector2f& center, float size);
  void drawSprite(const sf::Texture& texture, const sf::Vector2f& center, float size,
                  sf::Uint8 alpha = 255, float rotation = 0.f);

  sf::Vector2f tileCenter(TileCoord tile) const;
  static float fieldTop();
  static int facingIndex(Direction facing);

  sf::RenderTarget& m_target;
  sf::Font m_font;
  SpriteSet m_sprites;
  std::string m_fontPath;
  bool m_hasFont = false;
  float m_time = 0.f;
  sf::Clock m_clock;
};

}  // namespace bb
