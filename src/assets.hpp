#pragma once

#include <SFML/Graphics.hpp>

#include <array>
#include <string>

namespace bb {

/// The 16x16 pixel-art sprites BomberBoy is drawn with. They live in
/// `assets/sprites` and can be regenerated with `tools/generate_assets.cpp`;
/// when the files are missing the renderer falls back to vector shapes.
struct SpriteSet {
  static constexpr int kBombFrames = 3;
  static constexpr int kFlameFrames = 2;
  static constexpr int kFacings = 4;   // down, up, left, right
  static constexpr int kPlayers = 2;
  static constexpr int kPowerUps = 3;  // extra bomb, extra range, extra speed

  sf::Texture floor;
  sf::Texture wall;
  sf::Texture brick;
  std::array<sf::Texture, kBombFrames> bomb;
  std::array<sf::Texture, kFlameFrames> flameCenter;
  std::array<sf::Texture, kFlameFrames> flameHorizontal;
  std::array<sf::Texture, kFlameFrames> flameVertical;
  std::array<std::array<sf::Texture, kFacings>, kPlayers> player;
  std::array<sf::Texture, kPowerUps> powerUp;

  /// Looks for the sprite directory and loads every PNG. Returns false (and leaves
  /// the set unusable) when the art is not installed.
  bool load();

  bool ready() const { return m_ready; }
  const std::string& directory() const { return m_directory; }

 private:
  bool loadFrom(const std::string& directory);

  bool m_ready = false;
  std::string m_directory;
};

}  // namespace bb
