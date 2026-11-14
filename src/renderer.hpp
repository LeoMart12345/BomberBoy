#pragma once

#include <SFML/Graphics.hpp>

#include <string>

#include "assets.hpp"
#include "game.hpp"

namespace bb {

/// Draws a Game with the pixel-art sprites from `assets/sprites`, falling back to
/// plain SFML shapes when the art files are missing.
class Renderer {
 public:
  static constexpr float kTileSize = 48.f;
  static constexpr unsigned kHudHeight = 64;
  static constexpr unsigned kFooterHeight = 30;
  static constexpr unsigned kWindowWidth = static_cast<unsigned>(kCols * kTileSize);
  static constexpr unsigned kWindowHeight =
      static_cast<unsigned>(kRows * kTileSize) + kHudHeight + kFooterHeight;

  explicit Renderer(sf::RenderTarget& target);

  void draw(const Game& game, bool paused);

  /// False when no usable system font was found (text is then skipped).
  bool hasFont() const { return m_hasFont; }
  /// False when the sprite set is missing and the shape fallback is in use.
  bool hasSprites() const { return m_sprites.ready(); }
  const std::string& spriteDirectory() const { return m_sprites.directory(); }

 private:
  void drawFloor();
  void drawTiles(const Game& game);
  void drawPowerUps(const Game& game);
  void drawBombs(const Game& game);
  void drawFlames(const Game& game);
  void drawPlayers(const Game& game);
  void drawBombMarkers(const Game& game);
  void drawPlayer(const Player& player, int index);
  void drawHud(const Game& game);
  void drawFooter();
  void drawOverlays(const Game& game, bool paused);

  void drawText(const std::string& text, unsigned size, float x, float y, sf::Color color,
                bool centered = true, bool bold = true);
  void drawPip(float x, float y, sf::Color color, bool filled);
  void drawSprite(const sf::Texture& texture, const sf::Vector2f& center, float size,
                  sf::Uint8 alpha = 255, float rotation = 0.f);

  sf::Vector2f tileCenter(TileCoord tile) const;
  static float fieldTop();
  static int facingIndex(Direction facing);

  sf::RenderTarget& m_target;
  sf::Font m_font;
  SpriteSet m_sprites;
  bool m_hasFont = false;
  float m_time = 0.f;
  sf::Clock m_clock;
};

}  // namespace bb
