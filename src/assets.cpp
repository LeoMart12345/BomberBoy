#include "assets.hpp"

#include <cstdlib>
#include <fstream>
#include <vector>

namespace bb {
namespace {

bool fileExists(const std::string& path) {
  std::ifstream file(path);
  return file.good();
}

bool loadTexture(sf::Texture& texture, const std::string& path) {
  if (!texture.loadFromFile(path)) {
    return false;
  }
  // Pixel art must stay crisp when it is blown up to tile size.
  texture.setSmooth(false);
  return true;
}

const char* const kFacingNames[4] = {"down", "up", "left", "right"};

}  // namespace

bool SpriteSet::load() {
  std::vector<std::string> candidates;
  if (const char* custom = std::getenv("BOMBERBOY_ASSETS")) {
    candidates.emplace_back(custom);
  }
  candidates.emplace_back("assets/sprites");
  candidates.emplace_back("../assets/sprites");
  candidates.emplace_back("../../assets/sprites");

  for (const std::string& directory : candidates) {
    if (!fileExists(directory + "/floor.png")) {
      continue;
    }
    if (loadFrom(directory)) {
      m_directory = directory;
      m_ready = true;
      return true;
    }
  }

  m_directory.clear();
  m_ready = false;
  return false;
}

bool SpriteSet::loadFrom(const std::string& directory) {
  const auto path = [&directory](const std::string& name) { return directory + "/" + name; };

  if (!loadTexture(floor, path("floor.png")) || !loadTexture(wall, path("wall.png")) ||
      !loadTexture(brick, path("brick.png"))) {
    return false;
  }

  for (int frame = 0; frame < kBombFrames; ++frame) {
    if (!loadTexture(bomb[static_cast<std::size_t>(frame)],
                     path("bomb" + std::to_string(frame) + ".png"))) {
      return false;
    }
  }

  for (int frame = 0; frame < kFlameFrames; ++frame) {
    const std::string suffix = std::to_string(frame) + ".png";
    const auto index = static_cast<std::size_t>(frame);
    if (!loadTexture(flameCenter[index], path("flame_center" + suffix)) ||
        !loadTexture(flameHorizontal[index], path("flame_h" + suffix)) ||
        !loadTexture(flameVertical[index], path("flame_v" + suffix))) {
      return false;
    }
  }

  for (int index = 0; index < kPlayers; ++index) {
    for (int facing = 0; facing < kFacings; ++facing) {
      const std::string name = "player" + std::to_string(index + 1) + "_" + kFacingNames[facing] +
                               ".png";
      if (!loadTexture(player[static_cast<std::size_t>(index)][static_cast<std::size_t>(facing)],
                       path(name))) {
        return false;
      }
    }
  }

  if (!loadTexture(powerUp[0], path("powerup_bomb.png")) ||
      !loadTexture(powerUp[1], path("powerup_range.png")) ||
      !loadTexture(powerUp[2], path("powerup_speed.png"))) {
    return false;
  }

  return true;
}

}  // namespace bb
