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

/// Reads the size fields straight out of the PNG header. The renderer only ever
/// needs square tiles, and parsing the file is enough to spot a renamed or
/// truncated sprite without opening a window.
struct PngHeader {
  bool valid = false;
  unsigned width = 0;
  unsigned height = 0;
};

PngHeader readPngHeader(const std::string& path) {
  PngHeader header;

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return header;
  }

  unsigned char bytes[24] = {};
  file.read(reinterpret_cast<char*>(bytes), static_cast<std::streamsize>(sizeof(bytes)));
  if (file.gcount() != static_cast<std::streamsize>(sizeof(bytes))) {
    return header;
  }

  const unsigned char kSignature[8] = {0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au};
  for (int index = 0; index < 8; ++index) {
    if (bytes[index] != kSignature[index]) {
      return header;
    }
  }

  // The first chunk of every PNG is IHDR, followed by width and height.
  if (bytes[12] != 0x49u || bytes[13] != 0x48u || bytes[14] != 0x44u || bytes[15] != 0x52u) {
    return header;
  }

  const auto bigEndian = [&bytes](int offset) {
    return (static_cast<unsigned>(bytes[offset]) << 24) |
           (static_cast<unsigned>(bytes[offset + 1]) << 16) |
           (static_cast<unsigned>(bytes[offset + 2]) << 8) |
           static_cast<unsigned>(bytes[offset + 3]);
  };

  header.width = bigEndian(16);
  header.height = bigEndian(20);
  header.valid = true;
  return header;
}

}  // namespace

std::string SpriteSet::locateDirectory() {
  std::vector<std::string> candidates;
  if (const char* custom = std::getenv("BOMBERBOY_ASSETS")) {
    candidates.emplace_back(custom);
  }
  candidates.emplace_back("assets/sprites");
  candidates.emplace_back("../assets/sprites");
  candidates.emplace_back("../../assets/sprites");

  for (const std::string& directory : candidates) {
    if (fileExists(directory + "/floor.png")) {
      return directory;
    }
  }
  return {};
}

std::vector<std::string> SpriteSet::expectedFiles() {
  std::vector<std::string> files = {"floor.png", "wall.png", "brick.png"};

  for (int frame = 0; frame < kBombFrames; ++frame) {
    files.push_back("bomb" + std::to_string(frame) + ".png");
  }

  for (int frame = 0; frame < kFlameFrames; ++frame) {
    const std::string suffix = std::to_string(frame) + ".png";
    files.push_back("flame_center" + suffix);
    files.push_back("flame_h" + suffix);
    files.push_back("flame_v" + suffix);
  }

  for (int index = 0; index < kPlayers; ++index) {
    for (int facing = 0; facing < kFacings; ++facing) {
      files.push_back("player" + std::to_string(index + 1) + "_" + kFacingNames[facing] + ".png");
    }
  }

  files.push_back("powerup_bomb.png");
  files.push_back("powerup_range.png");
  files.push_back("powerup_speed.png");
  return files;
}

SpriteAudit SpriteSet::audit(const std::string& directory) {
  SpriteAudit result;
  const std::vector<std::string> files = expectedFiles();
  result.expected = static_cast<int>(files.size());

  for (const std::string& name : files) {
    const std::string path = directory + "/" + name;
    if (!fileExists(path)) {
      result.missing.push_back(name);
      continue;
    }

    const PngHeader header = readPngHeader(path);
    if (!header.valid) {
      result.malformed.push_back(name + " is not a PNG");
    } else if (header.width == 0 || header.width != header.height) {
      result.malformed.push_back(name + " is " + std::to_string(header.width) + "x" +
                                 std::to_string(header.height) + ", expected a square image");
    }
  }

  return result;
}

bool SpriteSet::load() {
  const std::string directory = locateDirectory();
  if (directory.empty() || !loadFrom(directory)) {
    m_directory.clear();
    m_ready = false;
    return false;
  }

  m_directory = directory;
  m_ready = true;
  return true;
}

bool SpriteSet::loadFrom(const std::string& directory) {
  std::vector<sf::Texture*> slots = textureSlots();
  const std::vector<std::string> files = expectedFiles();
  if (slots.size() != files.size()) {
    return false;
  }

  for (std::size_t index = 0; index < files.size(); ++index) {
    if (!loadTexture(*slots[index], directory + "/" + files[index])) {
      return false;
    }
  }

  return true;
}

std::vector<sf::Texture*> SpriteSet::textureSlots() {
  std::vector<sf::Texture*> slots = {&floor, &wall, &brick};

  for (sf::Texture& texture : bomb) {
    slots.push_back(&texture);
  }

  for (std::size_t frame = 0; frame < flameCenter.size(); ++frame) {
    slots.push_back(&flameCenter[frame]);
    slots.push_back(&flameHorizontal[frame]);
    slots.push_back(&flameVertical[frame]);
  }

  for (auto& playerFacings : player) {
    for (sf::Texture& texture : playerFacings) {
      slots.push_back(&texture);
    }
  }

  for (sf::Texture& texture : powerUp) {
    slots.push_back(&texture);
  }

  return slots;
}

}  // namespace bb
