// Generates the pixel-art sprite set that BomberBoy ships with. Every sprite is
// 16x16 pixels and is drawn at 3x zoom by the renderer, which keeps the art
// crisp and lets anybody swap in their own PNGs with the same names and size.
//
// Usage: generate_assets [output-directory]   (default: assets/sprites)

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr int kSize = 16;

sf::Color rgb(int r, int g, int b, int a = 255) {
  return sf::Color(static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
                   static_cast<std::uint8_t>(b), static_cast<std::uint8_t>(a));
}

enum class Facing { Down = 0, Up = 1, Left = 2, Right = 3 };

/// A tiny 16x16 pixel canvas with alpha blending, just enough to hand-build the
/// sprite set without an art tool.
class Canvas {
 public:
  Canvas() : m_image(sf::Vector2u{kSize, kSize}, sf::Color(0, 0, 0, 0)) {}

  void pixel(int x, int y, sf::Color colour) {
    if (x < 0 || y < 0 || x >= kSize || y >= kSize || colour.a == 0) {
      return;
    }
    if (colour.a == 255) {
      m_image.setPixel({static_cast<unsigned>(x), static_cast<unsigned>(y)}, colour);
      return;
    }
    const sf::Color under = m_image.getPixel({static_cast<unsigned>(x), static_cast<unsigned>(y)});
    const float src = colour.a / 255.f;
    const float dst = under.a / 255.f;
    const float out = src + dst * (1.f - src);
    if (out <= 0.f) {
      m_image.setPixel({static_cast<unsigned>(x), static_cast<unsigned>(y)}, sf::Color(0, 0, 0, 0));
      return;
    }
    const auto mix = [&](std::uint8_t s, std::uint8_t d) {
      const float value = (s * src + d * dst * (1.f - src)) / out;
      return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.f, 255.f)));
    };
    m_image.setPixel({static_cast<unsigned>(x), static_cast<unsigned>(y)},
                     sf::Color(mix(colour.r, under.r), mix(colour.g, under.g),
                               mix(colour.b, under.b),
                               static_cast<std::uint8_t>(std::lround(out * 255.f))));
  }

  void rect(int x, int y, int w, int h, sf::Color colour) {
    for (int yy = y; yy < y + h; ++yy) {
      for (int xx = x; xx < x + w; ++xx) {
        pixel(xx, yy, colour);
      }
    }
  }

  void outline(int x, int y, int w, int h, sf::Color colour) {
    for (int xx = x; xx < x + w; ++xx) {
      pixel(xx, y, colour);
      pixel(xx, y + h - 1, colour);
    }
    for (int yy = y; yy < y + h; ++yy) {
      pixel(x, yy, colour);
      pixel(x + w - 1, yy, colour);
    }
  }

  void hLine(int x0, int x1, int y, sf::Color colour) {
    for (int x = x0; x <= x1; ++x) {
      pixel(x, y, colour);
    }
  }

  void vLine(int x, int y0, int y1, sf::Color colour) {
    for (int y = y0; y <= y1; ++y) {
      pixel(x, y, colour);
    }
  }

  void disc(float cx, float cy, float radius, sf::Color colour) {
    const int x0 = static_cast<int>(std::floor(cx - radius));
    const int x1 = static_cast<int>(std::ceil(cx + radius));
    const int y0 = static_cast<int>(std::floor(cy - radius));
    const int y1 = static_cast<int>(std::ceil(cy + radius));
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const float dx = static_cast<float>(x) + 0.5f - cx;
        const float dy = static_cast<float>(y) + 0.5f - cy;
        if (dx * dx + dy * dy <= radius * radius) {
          pixel(x, y, colour);
        }
      }
    }
  }

  /// Clears the alpha channel so sprites can sit on top of the arena.
  void punch(int x, int y) {
    if (x < 0 || y < 0 || x >= kSize || y >= kSize) {
      return;
    }
    m_image.setPixel({static_cast<unsigned>(x), static_cast<unsigned>(y)}, sf::Color(0, 0, 0, 0));
  }

  bool save(const std::filesystem::path& path) const {
    if (!m_image.saveToFile(path)) {
      std::fprintf(stderr, "error: could not write %s\n", path.string().c_str());
      return false;
    }
    std::printf("  wrote %s\n", path.string().c_str());
    return true;
  }

 private:
  sf::Image m_image;
};

}  // namespace

namespace art {

Canvas floorTile() {
  Canvas c;
  c.rect(0, 0, kSize, kSize, rgb(26, 35, 47));
  // Slate checker keeps the tile grid readable without a hard line.
  c.rect(0, 0, 8, 8, rgb(29, 39, 52));
  c.rect(8, 8, 8, 8, rgb(29, 39, 52));
  static const int speckles[][2] = {{3, 4}, {6, 11}, {11, 3}, {13, 12}, {2, 13}, {9, 6}};
  for (const auto& speckle : speckles) {
    c.pixel(speckle[0], speckle[1], rgb(42, 56, 72));
  }
  c.pixel(4, 9, rgb(17, 23, 32));
  c.pixel(12, 7, rgb(17, 23, 32));
  // Bevel: lit top-left edge, shaded bottom-right edge.
  c.hLine(0, kSize - 1, 0, rgb(37, 49, 63));
  c.vLine(0, 0, kSize - 1, rgb(34, 45, 59));
  c.hLine(0, kSize - 1, kSize - 1, rgb(17, 23, 32));
  c.vLine(kSize - 1, 0, kSize - 1, rgb(19, 26, 36));
  return c;
}

Canvas wallTile() {
  Canvas c;
  c.rect(0, 0, kSize, kSize, rgb(42, 51, 65));
  c.rect(1, 1, 14, 14, rgb(96, 110, 131));
  c.rect(2, 2, 12, 12, rgb(110, 126, 148));
  c.hLine(1, 14, 1, rgb(150, 165, 187));
  c.vLine(1, 1, 14, rgb(138, 153, 175));
  c.hLine(1, 14, 14, rgb(70, 82, 100));
  c.vLine(14, 1, 14, rgb(78, 91, 110));
  c.outline(5, 5, 6, 6, rgb(86, 100, 120));
  c.rect(6, 6, 4, 4, rgb(120, 136, 158));
  c.hLine(6, 9, 6, rgb(140, 156, 178));
  c.pixel(3, 3, rgb(162, 178, 200));
  c.pixel(12, 3, rgb(146, 162, 184));
  c.pixel(3, 12, rgb(72, 84, 102));
  c.pixel(12, 12, rgb(64, 76, 93));
  return c;
}

Canvas brickTile() {
  Canvas c;
  c.rect(0, 0, kSize, kSize, rgb(86, 48, 27));
  const auto brickRow = [&c](int y0, int height, const std::vector<int>& joints) {
    c.rect(0, y0, kSize, height, rgb(170, 100, 54));
    c.hLine(0, kSize - 1, y0, rgb(200, 128, 78));
    c.hLine(0, kSize - 1, y0 + height - 1, rgb(128, 68, 34));
    for (const int joint : joints) {
      c.vLine(joint, y0, y0 + height - 1, rgb(86, 48, 27));
      c.pixel(joint, y0, rgb(186, 112, 64));
    }
  };
  brickRow(0, 5, {5, 11});
  brickRow(5, 5, {2, 8, 14});
  brickRow(10, 6, {5, 11});
  c.outline(0, 0, kSize, kSize, rgb(64, 35, 19));
  c.pixel(3, 1, rgb(214, 146, 92));
  c.pixel(9, 6, rgb(214, 146, 92));
  c.pixel(13, 11, rgb(206, 138, 86));
  return c;
}

Canvas bombSprite(int frame) {
  Canvas c;
  // Fuse first so the body overlaps its bottom end.
  c.pixel(10, 4, rgb(126, 100, 66));
  c.pixel(11, 4, rgb(104, 82, 54));
  c.pixel(11, 3, rgb(150, 120, 80));
  c.pixel(12, 2, rgb(112, 88, 58));
  c.pixel(10, 3, rgb(112, 88, 58));

  c.disc(8.f, 9.f, 6.5f, rgb(12, 14, 20));
  c.disc(8.f, 9.f, 5.7f, rgb(28, 32, 44));
  c.disc(8.f, 9.6f, 4.4f, rgb(35, 40, 54));
  c.disc(5.9f, 6.9f, 1.9f, rgb(120, 136, 160, 150));
  c.pixel(5, 6, rgb(238, 246, 253));
  c.pixel(6, 6, rgb(206, 222, 240));
  c.pixel(4, 8, rgb(150, 168, 192));
  c.pixel(3, 10, rgb(84, 98, 120));
  c.pixel(12, 12, rgb(18, 21, 30));

  static const int sparkX[3] = {12, 13, 11};
  static const int sparkY[3] = {2, 1, 3};
  const int sx = sparkX[frame % 3];
  const int sy = sparkY[frame % 3];
  c.disc(static_cast<float>(sx) + 0.5f, static_cast<float>(sy) + 0.5f, 2.1f,
         rgb(255, 142, 32, 200));
  c.disc(static_cast<float>(sx) + 0.5f, static_cast<float>(sy) + 0.5f, 1.1f,
         rgb(255, 248, 206));
  return c;
}

Canvas flameSprite(bool horizontal, int frame) {
  Canvas c;
  const int wobble = frame == 0 ? 0 : 1;
  if (horizontal) {
    c.rect(0, 2 + wobble, kSize, 12 - wobble, rgb(255, 86, 16, 205));
    c.rect(0, 4, kSize, 8, rgb(255, 134, 26, 232));
    c.rect(0, 5, kSize, 6, rgb(255, 184, 52, 244));
    c.rect(0, 6, kSize, 4, rgb(255, 228, 122, 248));
    c.hLine(1, 14, 7, rgb(255, 250, 226));
    c.hLine(2, 13, 8, rgb(255, 236, 168));
    c.punch(0, 2 + wobble);
    c.punch(15, 2 + wobble);
    c.punch(0, 13);
    c.punch(15, 13);
    if (frame == 1) {
      c.pixel(1, 4, rgb(255, 120, 24));
      c.pixel(14, 10, rgb(255, 150, 34));
      c.pixel(7, 1, rgb(255, 100, 20));
      c.pixel(9, 14, rgb(255, 160, 42));
    }
  } else {
    c.rect(2 + wobble, 0, 12 - wobble, kSize, rgb(255, 86, 16, 205));
    c.rect(4, 0, 8, kSize, rgb(255, 134, 26, 232));
    c.rect(5, 0, 6, kSize, rgb(255, 184, 52, 244));
    c.rect(6, 0, 4, kSize, rgb(255, 228, 122, 248));
    c.vLine(7, 1, 14, rgb(255, 250, 226));
    c.vLine(8, 2, 13, rgb(255, 236, 168));
    c.punch(2 + wobble, 0);
    c.punch(2 + wobble, 15);
    c.punch(13, 0);
    c.punch(13, 15);
    if (frame == 1) {
      c.pixel(4, 1, rgb(255, 120, 24));
      c.pixel(10, 14, rgb(255, 150, 34));
      c.pixel(1, 7, rgb(255, 100, 20));
      c.pixel(14, 9, rgb(255, 160, 42));
    }
  }
  return c;
}

Canvas playerSprite(int player, Facing facing) {
  const sf::Color suit = player == 1 ? rgb(238, 116, 74) : rgb(74, 178, 240);
  const sf::Color suitDark = player == 1 ? rgb(158, 60, 30) : rgb(28, 96, 150);
  const sf::Color suitLight = player == 1 ? rgb(255, 172, 136) : rgb(158, 224, 255);
  const sf::Color skin = rgb(248, 238, 218);
  const sf::Color skinShade = rgb(206, 188, 160);
  const sf::Color ink = rgb(26, 30, 40);

  Canvas c;
  // Head shell.
  c.disc(8.f, 7.5f, 6.7f, ink);
  c.disc(8.f, 7.5f, 6.0f, suit);
  c.disc(6.2f, 5.4f, 2.4f, suitLight);
  c.pixel(5, 5, rgb(255, 255, 255));
  c.disc(10.6f, 10.4f, 1.8f, suitDark);

  // Antenna on top of the shell so it is always visible.
  c.pixel(8, 0, ink);
  c.pixel(9, 1, ink);
  c.disc(8.6f, 1.2f, 1.7f, suitLight);
  c.pixel(8, 0, rgb(255, 255, 255));

  // Face plate, shifted for the direction the bomber is looking.
  if (facing == Facing::Up) {
    // Back of the helmet: strap, buckle and a vent instead of a face.
    c.rect(4, 5, 8, 2, suitDark);
    c.hLine(4, 11, 5, ink);
    c.rect(6, 9, 4, 3, suitDark);
    c.hLine(7, 8, 10, suitLight);
  } else {
    const int shift = facing == Facing::Left ? -1 : facing == Facing::Right ? 1 : 0;
    c.rect(4 + shift, 5, 8, 6, ink);
    c.rect(5 + shift, 6, 6, 4, skin);
    c.hLine(5 + shift, 10 + shift, 9, skinShade);
    if (facing == Facing::Down) {
      c.rect(6, 7, 2, 2, ink);
      c.rect(9, 7, 2, 2, ink);
    } else if (facing == Facing::Left) {
      c.rect(4, 7, 2, 2, ink);
      c.rect(7, 7, 2, 2, ink);
    } else {
      c.rect(7, 7, 2, 2, ink);
      c.rect(10, 7, 2, 2, ink);
    }
  }

  // Shoes, drawn last so they sit under the shell.
  c.rect(3, 12, 4, 3, ink);
  c.rect(9, 12, 4, 3, ink);
  c.rect(4, 12, 3, 2, suitDark);
  c.rect(9, 12, 3, 2, suitDark);
  c.hLine(4, 6, 14, suit);
  c.hLine(9, 11, 14, suit);
  return c;
}

Canvas powerUpSprite(int kind) {
  const sf::Color accent =
      kind == 0 ? rgb(96, 190, 246) : kind == 1 ? rgb(250, 124, 52) : rgb(250, 198, 58);
  Canvas c;
  c.rect(1, 1, 14, 14, accent);
  c.rect(2, 2, 12, 12, rgb(246, 249, 253));
  c.punch(1, 1);
  c.punch(14, 1);
  c.punch(1, 14);
  c.punch(14, 14);
  c.hLine(2, 13, 2, rgb(255, 255, 255));
  c.hLine(2, 13, 13, rgb(204, 214, 228));
  c.vLine(2, 3, 12, rgb(255, 255, 255));
  c.vLine(13, 3, 12, rgb(204, 214, 228));

  if (kind == 0) {
    // Extra bomb.
    c.pixel(9, 4, rgb(126, 100, 66));
    c.pixel(10, 3, rgb(214, 158, 62));
    c.disc(8.f, 9.2f, 4.4f, rgb(26, 30, 40));
    c.disc(8.f, 9.4f, 3.2f, rgb(46, 52, 68));
    c.disc(6.6f, 7.8f, 1.2f, rgb(214, 228, 244));
    c.pixel(6, 7, rgb(255, 255, 255));
  } else if (kind == 1) {
    // Extra blast range.
    c.disc(8.f, 9.6f, 4.2f, rgb(238, 88, 20));
    c.disc(8.f, 10.f, 3.0f, rgb(255, 156, 38));
    c.disc(8.f, 10.6f, 1.7f, rgb(255, 236, 150));
    c.pixel(8, 4, rgb(255, 196, 70));
    c.pixel(8, 5, rgb(255, 150, 34));
    c.pixel(6, 6, rgb(255, 120, 24));
    c.pixel(10, 6, rgb(255, 120, 24));
    c.pixel(8, 7, rgb(255, 214, 96));
  } else {
    // Extra speed: a lightning bolt.
    c.hLine(9, 11, 3, rgb(250, 198, 58));
    c.hLine(8, 10, 4, rgb(252, 214, 84));
    c.hLine(7, 9, 5, rgb(252, 214, 84));
    c.hLine(6, 8, 6, rgb(252, 214, 84));
    c.hLine(5, 10, 7, rgb(255, 236, 132));
    c.hLine(6, 8, 8, rgb(248, 190, 46));
    c.hLine(5, 7, 9, rgb(248, 190, 46));
    c.hLine(4, 6, 10, rgb(240, 176, 36));
    c.hLine(4, 6, 11, rgb(240, 176, 36));
    c.hLine(4, 5, 12, rgb(226, 158, 28));
  }
  return c;
}

Canvas flameCenterSprite(int frame) {
  Canvas c;
  const float pulse = frame == 0 ? 0.f : 0.5f;
  c.disc(8.f, 8.f, 7.4f + pulse, rgb(255, 84, 16, 210));
  c.disc(8.f, 8.f, 6.0f + pulse, rgb(255, 140, 28, 235));
  c.disc(8.f, 8.f, 4.4f, rgb(255, 196, 62, 245));
  c.disc(8.f, 8.f, 2.6f, rgb(255, 240, 170, 250));
  c.disc(8.f, 8.f, 1.2f, rgb(255, 255, 246));
  c.pixel(8, 0, rgb(255, 124, 26));
  c.pixel(8, 15, rgb(255, 110, 22));
  c.pixel(0, 8, rgb(255, 124, 26));
  c.pixel(15, 8, rgb(255, 110, 22));
  if (frame == 1) {
    c.pixel(3, 2, rgb(255, 140, 30));
    c.pixel(12, 13, rgb(255, 150, 36));
    c.pixel(2, 12, rgb(255, 120, 24));
    c.pixel(13, 3, rgb(255, 132, 28));
  }
  return c;
}

}  // namespace art

int main(int argc, char** argv) {
  const std::filesystem::path output = argc > 1 ? argv[1] : "assets/sprites";
  std::error_code error;
  std::filesystem::create_directories(output, error);
  if (error) {
    std::fprintf(stderr, "error: could not create %s\n", output.string().c_str());
    return 1;
  }

  std::printf("writing BomberBoy sprites to %s\n", output.string().c_str());

  bool ok = true;
  ok = art::floorTile().save(output / "floor.png") && ok;
  ok = art::wallTile().save(output / "wall.png") && ok;
  ok = art::brickTile().save(output / "brick.png") && ok;

  for (int frame = 0; frame < 3; ++frame) {
    ok = art::bombSprite(frame).save(output / ("bomb" + std::to_string(frame) + ".png")) && ok;
  }
  for (int frame = 0; frame < 2; ++frame) {
    const std::string suffix = std::to_string(frame) + ".png";
    ok = art::flameCenterSprite(frame).save(output / ("flame_center" + suffix)) && ok;
    ok = art::flameSprite(true, frame).save(output / ("flame_h" + suffix)) && ok;
    ok = art::flameSprite(false, frame).save(output / ("flame_v" + suffix)) && ok;
  }

  static const char* kFacings[4] = {"down", "up", "left", "right"};
  for (int player = 1; player <= 2; ++player) {
    for (int facing = 0; facing < 4; ++facing) {
      const std::string name =
          "player" + std::to_string(player) + "_" + kFacings[facing] + ".png";
      ok = art::playerSprite(player, static_cast<Facing>(facing)).save(output / name) && ok;
    }
  }

  ok = art::powerUpSprite(0).save(output / "powerup_bomb.png") && ok;
  ok = art::powerUpSprite(1).save(output / "powerup_range.png") && ok;
  ok = art::powerUpSprite(2).save(output / "powerup_speed.png") && ok;

  if (!ok) {
    std::fprintf(stderr, "error: some sprites could not be written\n");
    return 1;
  }
  std::printf("done\n");
  return 0;
}
