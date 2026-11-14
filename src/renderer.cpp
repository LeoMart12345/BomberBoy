#include "renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <fstream>

namespace bb {
namespace {

constexpr float kPi = 3.14159265358979f;

const sf::Color kBackground(13, 18, 26);
const sf::Color kHudBackground(9, 14, 20);
const sf::Color kHudLine(40, 54, 72);
const sf::Color kFloorA(24, 33, 45);
const sf::Color kFloorB(20, 28, 39);
const sf::Color kFloorLine(255, 255, 255, 8);
const sf::Color kWallFill(66, 79, 97);
const sf::Color kWallLight(104, 120, 142);
const sf::Color kWallDark(35, 43, 56);
const sf::Color kBrickFill(154, 88, 47);
const sf::Color kBrickMortar(104, 56, 30);
const sf::Color kBrickEdge(84, 44, 22);
const sf::Color kTextPrimary(228, 236, 246);
const sf::Color kTextMuted(126, 143, 162);

sf::Color playerColor(int index) {
  return index == 1 ? sf::Color(255, 118, 74) : sf::Color(86, 200, 255);
}

sf::Color playerDark(int index) {
  return index == 1 ? sf::Color(170, 60, 30) : sf::Color(34, 106, 150);
}

float clamp01(float value) { return std::clamp(value, 0.f, 1.f); }

sf::Uint8 alphaOf(float ratio, float maxAlpha) {
  return static_cast<sf::Uint8>(std::clamp(ratio, 0.f, 1.f) * maxAlpha);
}

bool readable(const char* path) {
  std::ifstream file(path);
  return file.good();
}

}  // namespace

Renderer::Renderer(sf::RenderTarget& target) : m_target(target) {
  static const std::array<const char*, 20> kFontCandidates = {
      "assets/font.ttf",
      "font.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
      "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf",
      "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
      "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
      "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Bold.ttf",
      "/usr/share/fonts/truetype/carlito/Carlito-Bold.ttf",
      "/usr/share/fonts/google-carlito-fonts/Carlito-Bold.ttf",
      "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
      "/usr/share/fonts/gnu-free/FreeSansBold.ttf",
      "/usr/share/fonts/truetype/open-sans/OpenSans-Bold.ttf",
      "/usr/share/fonts/open-sans/OpenSans-Bold.ttf",
      "/usr/share/fonts/cantarell/Cantarell-Bold.otf",
      "/usr/share/fonts/abattis-cantarell-vf-fonts/Cantarell-VF.otf",
      "/usr/share/fonts/adwaita-sans-fonts/AdwaitaSans-Bold.ttf",
      "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
      "/Library/Fonts/Arial Bold.ttf",
      "C:/Windows/Fonts/arialbd.ttf",
      "C:/Windows/Fonts/segoeuib.ttf"};

  if (const char* custom = std::getenv("BOMBERBOY_FONT")) {
    if (readable(custom) && m_font.loadFromFile(custom)) {
      m_hasFont = true;
    }
  }

  for (std::size_t i = 0; !m_hasFont && i < kFontCandidates.size(); ++i) {
    const char* path = kFontCandidates[i];
    if (readable(path) && m_font.loadFromFile(path)) {
      m_hasFont = true;
    }
  }
}

float Renderer::fieldTop() { return static_cast<float>(kHudHeight); }

sf::Vector2f Renderer::tileCenter(TileCoord tile) const {
  return sf::Vector2f{static_cast<float>(tile.x) * kTileSize + kTileSize * 0.5f,
                      fieldTop() + static_cast<float>(tile.y) * kTileSize + kTileSize * 0.5f};
}

void Renderer::drawText(const std::string& text, unsigned size, float x, float y, sf::Color color,
                        bool centered, bool bold) {
  if (!m_hasFont) {
    return;
  }
  sf::Text label;
  label.setFont(m_font);
  label.setString(text);
  label.setCharacterSize(size);
  label.setFillColor(color);
  if (bold) {
    label.setStyle(sf::Text::Bold);
  }
  if (centered) {
    const sf::FloatRect bounds = label.getLocalBounds();
    label.setPosition(std::round(x - bounds.left - bounds.width * 0.5f),
                      std::round(y - bounds.top - bounds.height * 0.5f));
  } else {
    label.setPosition(std::round(x), std::round(y));
  }
  m_target.draw(label);
}

void Renderer::drawPip(float x, float y, sf::Color color, bool filled) {
  constexpr float kRadius = 6.f;
  sf::CircleShape pip(kRadius, 20);
  pip.setOrigin(kRadius, kRadius);
  pip.setPosition(x, y);
  if (filled) {
    pip.setFillColor(color);
  } else {
    pip.setFillColor(sf::Color(color.r, color.g, color.b, 26));
    pip.setOutlineThickness(1.5f);
    pip.setOutlineColor(sf::Color(color.r, color.g, color.b, 120));
  }
  m_target.draw(pip);
}

void Renderer::draw(const Game& game, bool paused) {
  m_time += std::clamp(m_clock.restart().asSeconds(), 0.f, 0.1f);

  m_target.clear(kBackground);
  drawFloor();
  drawTiles(game);
  drawPowerUps(game);
  drawBombs(game);
  drawFlames(game);
  drawPlayers(game);
  drawHud(game);
  drawFooter();
  drawOverlays(game, paused);
}

void Renderer::drawFloor() {
  sf::RectangleShape tile(sf::Vector2f(kTileSize, kTileSize));
  sf::RectangleShape line(sf::Vector2f(kTileSize, 1.f));

  for (int y = 0; y < kRows; ++y) {
    for (int x = 0; x < kCols; ++x) {
      const float px = static_cast<float>(x) * kTileSize;
      const float py = fieldTop() + static_cast<float>(y) * kTileSize;
      tile.setPosition(px, py);
      tile.setFillColor((x + y) % 2 == 0 ? kFloorA : kFloorB);
      m_target.draw(tile);

      if (y + 1 < kRows) {
        line.setPosition(px, py + kTileSize);
        line.setFillColor(kFloorLine);
        m_target.draw(line);
      }
    }
  }
}

void Renderer::drawTiles(const Game& game) {
  constexpr float kInset = 1.f;
  constexpr float kBlockSize = kTileSize - 2.f * kInset;

  sf::RectangleShape block(sf::Vector2f(kBlockSize, kBlockSize));
  block.setOutlineThickness(1.f);
  sf::RectangleShape strip(sf::Vector2f(kBlockSize - 6.f, 3.f));
  sf::RectangleShape mortar(sf::Vector2f(kBlockSize - 4.f, 2.f));
  sf::RectangleShape joint(sf::Vector2f(2.f, 12.f));

  for (int y = 0; y < kRows; ++y) {
    for (int x = 0; x < kCols; ++x) {
      const Tile tileType = game.tileAt(x, y);
      if (tileType == Tile::Floor) {
        continue;
      }

      const float px = static_cast<float>(x) * kTileSize + kInset;
      const float py = fieldTop() + static_cast<float>(y) * kTileSize + kInset;
      block.setPosition(px, py);

      if (tileType == Tile::Wall) {
        block.setFillColor(kWallFill);
        block.setOutlineColor(kWallDark);
        m_target.draw(block);

        strip.setPosition(px + 3.f, py + 3.f);
        strip.setFillColor(kWallLight);
        m_target.draw(strip);

        strip.setPosition(px + 3.f, py + kBlockSize - 6.f);
        strip.setFillColor(kWallDark);
        m_target.draw(strip);
      } else {
        block.setFillColor(kBrickFill);
        block.setOutlineColor(kBrickEdge);
        m_target.draw(block);

        mortar.setFillColor(kBrickMortar);
        mortar.setPosition(px + 2.f, py + 14.f);
        m_target.draw(mortar);
        mortar.setPosition(px + 2.f, py + 28.f);
        m_target.draw(mortar);

        joint.setFillColor(kBrickMortar);
        joint.setPosition(px + 12.f, py + 3.f);
        m_target.draw(joint);
        joint.setPosition(px + 30.f, py + 3.f);
        m_target.draw(joint);
        joint.setPosition(px + 21.f, py + 17.f);
        m_target.draw(joint);
        joint.setPosition(px + 6.f, py + 31.f);
        m_target.draw(joint);
        joint.setPosition(px + 36.f, py + 31.f);
        m_target.draw(joint);
      }
    }
  }
}

void Renderer::drawPowerUps(const Game& game) {
  for (const PowerUp& powerUp : game.powerUps()) {
    const sf::Vector2f center = tileCenter(powerUp.tile);
    const float phase = static_cast<float>(powerUp.tile.x * 3 + powerUp.tile.y * 5);
    const float bob = std::sin(m_time * 5.f + phase) * 2.f;
    const float size = kTileSize * 0.62f;

    sf::Color accent(255, 110, 60);
    if (powerUp.kind == PowerUpKind::ExtraBomb) {
      accent = sf::Color(120, 205, 255);
    } else if (powerUp.kind == PowerUpKind::Speed) {
      accent = sf::Color(255, 214, 84);
    }

    sf::CircleShape shadow(size * 0.5f, 24);
    shadow.setOrigin(size * 0.5f, size * 0.5f);
    shadow.setScale(0.9f, 0.35f);
    shadow.setPosition(center.x, center.y + size * 0.55f);
    shadow.setFillColor(sf::Color(0, 0, 0, 90));
    m_target.draw(shadow);

    sf::RectangleShape base(sf::Vector2f(size, size));
    base.setOrigin(size * 0.5f, size * 0.5f);
    base.setPosition(center.x, center.y + bob);
    base.setFillColor(sf::Color(240, 244, 250));
    base.setOutlineThickness(3.f);
    base.setOutlineColor(accent);
    m_target.draw(base);

    const float iconX = center.x;
    const float iconY = center.y + bob;

    if (powerUp.kind == PowerUpKind::ExtraBomb) {
      sf::CircleShape bomb(size * 0.22f, 20);
      bomb.setOrigin(size * 0.22f, size * 0.22f);
      bomb.setPosition(iconX, iconY + 1.f);
      bomb.setFillColor(sf::Color(34, 40, 54));
      m_target.draw(bomb);

      sf::CircleShape glint(size * 0.07f, 12);
      glint.setOrigin(size * 0.07f, size * 0.07f);
      glint.setPosition(iconX - size * 0.07f, iconY - size * 0.05f);
      glint.setFillColor(sf::Color(220, 235, 250));
      m_target.draw(glint);
    } else if (powerUp.kind == PowerUpKind::ExtraRange) {
      sf::RectangleShape arm(sf::Vector2f(size * 0.52f, size * 0.17f));
      arm.setOrigin(size * 0.26f, size * 0.085f);
      arm.setPosition(iconX, iconY);
      arm.setFillColor(sf::Color(255, 120, 40));
      m_target.draw(arm);

      sf::RectangleShape column(sf::Vector2f(size * 0.17f, size * 0.52f));
      column.setOrigin(size * 0.085f, size * 0.26f);
      column.setPosition(iconX, iconY);
      column.setFillColor(sf::Color(255, 120, 40));
      m_target.draw(column);

      sf::RectangleShape coreArm(sf::Vector2f(size * 0.26f, size * 0.08f));
      coreArm.setOrigin(size * 0.13f, size * 0.04f);
      coreArm.setPosition(iconX, iconY);
      coreArm.setFillColor(sf::Color(255, 232, 150));
      m_target.draw(coreArm);

      sf::RectangleShape coreColumn(sf::Vector2f(size * 0.08f, size * 0.26f));
      coreColumn.setOrigin(size * 0.04f, size * 0.13f);
      coreColumn.setPosition(iconX, iconY);
      coreColumn.setFillColor(sf::Color(255, 232, 150));
      m_target.draw(coreColumn);
    } else {
      sf::ConvexShape bolt(3);
      bolt.setPoint(0, sf::Vector2f(size * 0.06f, -size * 0.26f));
      bolt.setPoint(1, sf::Vector2f(-size * 0.18f, size * 0.05f));
      bolt.setPoint(2, sf::Vector2f(size * 0.16f, size * 0.05f));
      bolt.setPosition(iconX, iconY);
      bolt.setFillColor(sf::Color(250, 190, 40));
      m_target.draw(bolt);

      sf::ConvexShape boltTail(3);
      boltTail.setPoint(0, sf::Vector2f(-size * 0.04f, size * 0.26f));
      boltTail.setPoint(1, sf::Vector2f(size * 0.18f, -size * 0.05f));
      boltTail.setPoint(2, sf::Vector2f(-size * 0.16f, -size * 0.05f));
      boltTail.setPosition(iconX, iconY);
      boltTail.setFillColor(sf::Color(255, 210, 90));
      m_target.draw(boltTail);
    }
  }
}


void Renderer::drawBombs(const Game& game) {
  for (const Bomb& bomb : game.bombs()) {
    const sf::Vector2f center = tileCenter(bomb.tile);
    const float urgency = 1.f - clamp01(bomb.fuse / Game::kBombFuse);
    const float pulse = 1.f + 0.10f * std::sin(m_time * (7.f + urgency * 26.f) + bomb.tile.x);
    const float radius = kTileSize * 0.30f * pulse;

    sf::CircleShape shadow(radius, 28);
    shadow.setOrigin(radius, radius);
    shadow.setScale(1.f, 0.34f);
    shadow.setPosition(center.x, center.y + kTileSize * 0.30f);
    shadow.setFillColor(sf::Color(0, 0, 0, 110));
    m_target.draw(shadow);

    sf::RectangleShape fuse(sf::Vector2f(3.f, kTileSize * 0.20f));
    fuse.setOrigin(1.5f, kTileSize * 0.20f);
    fuse.setPosition(center.x + radius * 0.35f, center.y - radius * 0.55f);
    fuse.setRotation(28.f);
    fuse.setFillColor(sf::Color(206, 196, 176));
    m_target.draw(fuse);

    constexpr float kFuseAngle = 28.f * kPi / 180.f;
    const float fuseLength = kTileSize * 0.20f;
    const float sparkX = center.x + radius * 0.35f + std::sin(kFuseAngle) * fuseLength;
    const float sparkY = center.y - radius * 0.55f - std::cos(kFuseAngle) * fuseLength;
    const float sparkRadius = 3.5f + std::sin(m_time * 24.f + bomb.tile.y) * 1.4f;
    sf::CircleShape spark(sparkRadius, 12);
    spark.setOrigin(sparkRadius, sparkRadius);
    spark.setPosition(sparkX, sparkY);
    spark.setFillColor(sf::Color(255, 196, 74, 235));
    m_target.draw(spark);

    sf::CircleShape body(radius, 32);
    body.setOrigin(radius, radius);
    body.setPosition(center.x, center.y);
    body.setFillColor(sf::Color(30, 33, 44));
    body.setOutlineThickness(2.5f);
    body.setOutlineColor(sf::Color(12, 14, 20));
    m_target.draw(body);

    sf::CircleShape highlight(radius * 0.30f, 20);
    highlight.setOrigin(radius * 0.30f, radius * 0.30f);
    highlight.setPosition(center.x - radius * 0.34f, center.y - radius * 0.36f);
    highlight.setFillColor(sf::Color(255, 255, 255, 55));
    m_target.draw(highlight);
  }
}

void Renderer::drawFlames(const Game& game) {
  sf::ConvexShape glow;
  glow.setPointCount(4);
  glow.setPoint(0, sf::Vector2f(0.f, -1.f));
  glow.setPoint(1, sf::Vector2f(1.f, 0.f));
  glow.setPoint(2, sf::Vector2f(0.f, 1.f));
  glow.setPoint(3, sf::Vector2f(-1.f, 0.f));

  sf::CircleShape halo(1.f, 24);

  for (const Flame& flame : game.flames()) {
    const float ratio = clamp01(flame.lifetime > 0.f ? flame.remaining / flame.lifetime : 0.f);
    const sf::Vector2f center = tileCenter(flame.tile);
    const float wobble = std::sin(m_time * 18.f + static_cast<float>(flame.tile.x + flame.tile.y));

    halo.setRadius(kTileSize * 0.52f);
    halo.setOrigin(kTileSize * 0.52f, kTileSize * 0.52f);
    halo.setPosition(center);
    halo.setFillColor(sf::Color(255, 128, 40, alphaOf(ratio, 55)));
    m_target.draw(halo);

    const float width = (flame.horizontal ? 0.52f : 0.42f) * kTileSize;
    const float height = (flame.horizontal ? 0.42f : 0.52f) * kTileSize;
    const float flare = 1.f + 0.06f * wobble;

    for (int layer = 0; layer < 3; ++layer) {
      const float scale = 1.f - layer * 0.28f;
      glow.setScale(width * scale * flare, height * scale * flare);
      glow.setPosition(center.x, center.y);
      glow.setRotation(wobble * 6.f + static_cast<float>(layer) * 45.f);
      if (layer == 0) {
        glow.setFillColor(sf::Color(255, 122, 32, alphaOf(ratio, 235)));
      } else if (layer == 1) {
        glow.setFillColor(sf::Color(255, 206, 86, alphaOf(ratio, 245)));
      } else {
        glow.setFillColor(sf::Color(255, 250, 225, alphaOf(ratio, 250)));
      }
      m_target.draw(glow);
    }
  }
}


void Renderer::drawPlayers(const Game& game) {
  const auto& players = game.players();
  for (std::size_t i = 0; i < players.size(); ++i) {
    drawPlayer(players[i], static_cast<int>(i));
  }
}

void Renderer::drawPlayer(const Player& player, int index) {
  if (!player.alive) {
    return;
  }

  const float radius = kTileSize * 0.36f;
  const float bob = std::sin(m_time * 9.f + static_cast<float>(index) * 1.7f) * 1.5f;
  const float cx = player.pos.x * kTileSize;
  const float cy = fieldTop() + player.pos.y * kTileSize + bob;
  const sf::Color color = playerColor(index);
  const sf::Color dark = playerDark(index);

  sf::CircleShape shadow(radius, 24);
  shadow.setOrigin(radius, radius);
  shadow.setScale(1.f, 0.36f);
  shadow.setPosition(cx, fieldTop() + player.pos.y * kTileSize + kTileSize * 0.30f);
  shadow.setFillColor(sf::Color(0, 0, 0, 110));
  m_target.draw(shadow);

  sf::CircleShape body(radius, 32);
  body.setOrigin(radius, radius);
  body.setPosition(cx, cy);
  body.setFillColor(color);
  body.setOutlineThickness(3.f);
  body.setOutlineColor(dark);
  m_target.draw(body);

  sf::CircleShape shine(radius * 0.32f, 16);
  shine.setOrigin(radius * 0.32f, radius * 0.32f);
  shine.setPosition(cx - radius * 0.34f, cy - radius * 0.40f);
  shine.setFillColor(sf::Color(255, 255, 255, 70));
  m_target.draw(shine);

  float faceX = 0.f;
  float faceY = 0.f;
  if (player.facing == Direction::Up) {
    faceY = -1.f;
  } else if (player.facing == Direction::Down) {
    faceY = 1.f;
  } else if (player.facing == Direction::Left) {
    faceX = -1.f;
  } else {
    faceX = 1.f;
  }

  const float eyeRadius = radius * 0.23f;
  const float pupilRadius = radius * 0.11f;
  const float spread = radius * 0.36f;
  const float forward = radius * 0.34f;

  for (int eye = 0; eye < 2; ++eye) {
    const float side = eye == 0 ? -1.f : 1.f;
    const float eyeX = cx + faceX * forward + (faceX == 0.f ? side * spread : 0.f);
    const float eyeY = cy + faceY * forward + (faceY == 0.f ? side * spread : 0.f);

    sf::CircleShape white(eyeRadius, 16);
    white.setOrigin(eyeRadius, eyeRadius);
    white.setPosition(eyeX, eyeY);
    white.setFillColor(sf::Color(250, 252, 255));
    m_target.draw(white);

    sf::CircleShape pupil(pupilRadius, 12);
    pupil.setOrigin(pupilRadius, pupilRadius);
    pupil.setPosition(eyeX + faceX * pupilRadius * 0.7f, eyeY + faceY * pupilRadius * 0.7f);
    pupil.setFillColor(sf::Color(22, 26, 34));
    m_target.draw(pupil);
  }
}

void Renderer::drawHud(const Game& game) {
  const float width = static_cast<float>(kWindowWidth);

  sf::RectangleShape bar(sf::Vector2f(width, static_cast<float>(kHudHeight)));
  bar.setFillColor(kHudBackground);
  m_target.draw(bar);

  sf::RectangleShape line(sf::Vector2f(width, 2.f));
  line.setPosition(0.f, static_cast<float>(kHudHeight) - 2.f);
  line.setFillColor(kHudLine);
  m_target.draw(line);

  const auto& scores = game.scores();
  for (int i = 0; i < kPlayerCount; ++i) {
    const float blockX = i == 0 ? 116.f : width - 116.f;
    const sf::Color color = playerColor(i);
    drawText("PLAYER " + std::to_string(i + 1), 16, blockX, 20.f, color);
    for (int pip = 0; pip < game.roundsToWin(); ++pip) {
      const float pipX = blockX + (static_cast<float>(pip) - (game.roundsToWin() - 1) * 0.5f) * 20.f;
      drawPip(pipX, 46.f, color, pip < scores[static_cast<std::size_t>(i)]);
    }
  }

  drawText("ROUND " + std::to_string(game.roundNumber()), 16, width * 0.5f, 19.f, kTextPrimary);
  drawText("FIRST TO " + std::to_string(game.roundsToWin()), 11, width * 0.5f, 42.f,
           kTextMuted);
}


void Renderer::drawFooter() {
  const float width = static_cast<float>(kWindowWidth);
  const float top = static_cast<float>(kWindowHeight - kFooterHeight);

  sf::RectangleShape bar(sf::Vector2f(width, static_cast<float>(kFooterHeight)));
  bar.setPosition(0.f, top);
  bar.setFillColor(kHudBackground);
  m_target.draw(bar);

  sf::RectangleShape line(sf::Vector2f(width, 1.f));
  line.setPosition(0.f, top);
  line.setFillColor(kHudLine);
  m_target.draw(line);

  drawText("P1  WASD + SPACE    P2  ARROWS + ENTER    R  RESTART    P  PAUSE    ESC  QUIT", 11,
           width * 0.5f, top + static_cast<float>(kFooterHeight) * 0.5f, kTextMuted);
}

void Renderer::drawOverlays(const Game& game, bool paused) {
  const float width = static_cast<float>(kWindowWidth);
  const float fieldHeight = static_cast<float>(kRows) * kTileSize;
  const float centerY = fieldTop() + fieldHeight * 0.5f;

  if (game.state() == RoundState::RoundOver) {
    sf::RectangleShape panel(sf::Vector2f(width, fieldHeight));
    panel.setPosition(0.f, fieldTop());
    panel.setFillColor(sf::Color(6, 10, 15, 208));
    m_target.draw(panel);

    std::string title;
    std::string subtitle;
    sf::Color titleColor = kTextPrimary;

    if (game.matchOver()) {
      const int winner = game.matchWinner();
      title = "PLAYER " + std::to_string(winner + 1) + " WINS THE MATCH!";
      subtitle = "Press R for a rematch";
      if (winner >= 0) {
        titleColor = playerColor(winner);
      }
    } else if (game.roundWinner() >= 0) {
      const int winner = game.roundWinner();
      title = "PLAYER " + std::to_string(winner + 1) + " TAKES THE ROUND";
      subtitle = "Press R for the next round";
      titleColor = playerColor(winner);
    } else {
      title = "DOUBLE KNOCKOUT";
      subtitle = "Press R to play the round again";
    }

    drawText(title, 30, width * 0.5f, centerY - 24.f, titleColor);

    const auto& scores = game.scores();
    const std::string scoreLine = "SCORE  " + std::to_string(scores[0]) + " - " +
                                  std::to_string(scores[1]);
    drawText(scoreLine, 15, width * 0.5f, centerY + 16.f, kTextMuted);
    drawText(subtitle, 14, width * 0.5f, centerY + 48.f, kTextPrimary);
  }

  if (paused) {
    sf::RectangleShape panel(sf::Vector2f(width, fieldHeight));
    panel.setPosition(0.f, fieldTop());
    panel.setFillColor(sf::Color(6, 10, 15, 140));
    m_target.draw(panel);

    drawText("PAUSED", 34, width * 0.5f, centerY - 12.f, kTextPrimary);
    drawText("Press P to resume", 14, width * 0.5f, centerY + 26.f, kTextMuted);
  }
}

}  // namespace bb

