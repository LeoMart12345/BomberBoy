#include "cli/cli.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>

namespace bb {
namespace {

struct OptionRow {
  const char* flags;
  const char* description;
};

/// One row per accepted option. `usageText()` prints this table and the parser
/// follows the same names, so a new flag shows up in `--help` as soon as it is
/// added here.
constexpr std::array<OptionRow, 9> kOptionRows = {{
    {"--selftest", "run the headless logic checks and exit"},
    {"--seed <n>", "build the arena from a fixed seed"},
    {"--cpu", "let the computer play player two"},
    {"--fps <n>", "frame cap, 0 runs uncapped (default 60)"},
    {"--scale <n>", "start with an n times bigger window (1-4)"},
    {"--fullscreen", "start in fullscreen"},
    {"--screenshot <file>", "render one frame to a PNG instead of opening a window"},
    {"--autoplay <s>", "scripted play seconds before the screenshot"},
    {"-h, --help", "show the help text"},
}};

std::string quoted(const std::string& text) { return "\"" + text + "\""; }

/// A number has to start with a digit or a sign: `strtol` happily skips leading
/// spaces, and " 7" is a typo rather than a seed.
bool startsLikeNumber(const std::string& text) {
  if (text.empty()) {
    return false;
  }
  const char first = text.front();
  return (first >= '0' && first <= '9') || first == '+' || first == '-';
}

/// Reads a whole number and rejects trailing junk such as "12x".
bool readInt(const std::string& text, long low, long high, long& out) {
  if (!startsLikeNumber(text)) {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const long parsed = std::strtol(text.c_str(), &end, 10);
  if (errno != 0 || end == text.c_str() || *end != '\0' || parsed < low || parsed > high) {
    return false;
  }
  out = parsed;
  return true;
}

/// Same rules for a decimal number; infinities and NaNs are refused.
bool readFloat(const std::string& text, float low, float high, float& out) {
  if (!startsLikeNumber(text)) {
    return false;
  }
  char* end = nullptr;
  const float parsed = std::strtof(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0' || !std::isfinite(parsed)) {
    return false;
  }
  if (parsed < low || parsed > high) {
    return false;
  }
  out = parsed;
  return true;
}

/// Seeds are unsigned, so a leading minus is an error and not a huge wrap-around.
bool readSeed(const std::string& text, std::uint32_t& out) {
  if (text.empty() || !std::all_of(text.begin(), text.end(),
                                   [](char c) { return c >= '0' && c <= '9'; })) {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
  if (errno != 0 || *end != '\0' ||
      parsed > static_cast<unsigned long long>(std::numeric_limits<std::uint32_t>::max())) {
    return false;
  }
  out = static_cast<std::uint32_t>(parsed);
  return true;
}

}  // namespace

ParsedOptions parseOptions(const std::vector<std::string>& arguments) {
  ParsedOptions result;
  Options& options = result.options;

  const auto fail = [&result](std::string message) {
    result.ok = false;
    result.error = std::move(message);
    return result;
  };
  const auto missing = [](const char* flag) { return std::string(flag) + " needs a value"; };

  for (std::size_t i = 0; i < arguments.size(); ++i) {
    const std::string& argument = arguments[i];
    const bool hasValue = i + 1 < arguments.size();

    if (argument == "--selftest" || argument == "--test") {
      options.runSelfTest = true;
    } else if (argument == "--cpu") {
      options.cpuOpponent = true;
    } else if (argument == "--fullscreen") {
      options.fullscreen = true;
    } else if (argument == "--help" || argument == "-h") {
      result.wantsHelp = true;
      return result;
    } else if (argument == "--seed") {
      if (!hasValue) {
        return fail(missing("--seed"));
      }
      if (!readSeed(arguments[i + 1], options.seed)) {
        return fail("--seed needs a whole number from 0 to 4294967295, got " +
                    quoted(arguments[i + 1]));
      }
      options.hasSeed = true;
      ++i;
    } else if (argument == "--fps") {
      if (!hasValue) {
        return fail(missing("--fps"));
      }
      long cap = 0;
      if (!readInt(arguments[i + 1], 0, kMaxFrameCap, cap)) {
        return fail("--fps needs a whole number from 0 to " + std::to_string(kMaxFrameCap) +
                    ", got " + quoted(arguments[i + 1]));
      }
      options.frameCap = static_cast<int>(cap);
      ++i;
    } else if (argument == "--scale") {
      if (!hasValue) {
        return fail(missing("--scale"));
      }
      long scale = 0;
      if (!readInt(arguments[i + 1], 1, kMaxWindowScale, scale)) {
        return fail("--scale needs a whole number from 1 to " + std::to_string(kMaxWindowScale) +
                    ", got " + quoted(arguments[i + 1]));
      }
      options.scale = static_cast<int>(scale);
      ++i;
    } else if (argument == "--autoplay") {
      if (!hasValue) {
        return fail(missing("--autoplay"));
      }
      if (!readFloat(arguments[i + 1], 0.f, kMaxAutoplay, options.autoplaySeconds)) {
        return fail("--autoplay needs a number of seconds from 0 to " +
                    std::to_string(static_cast<int>(kMaxAutoplay)) + ", got " +
                    quoted(arguments[i + 1]));
      }
      ++i;
    } else if (argument == "--screenshot") {
      if (!hasValue) {
        return fail(missing("--screenshot"));
      }
      if (arguments[i + 1].empty()) {
        return fail("--screenshot needs a file name");
      }
      options.screenshotPath = arguments[i + 1];
      ++i;
    } else {
      return fail("unknown option " + quoted(argument));
    }
  }

  return result;
}

std::string usageText() {
  std::size_t flagWidth = 0;
  for (const OptionRow& row : kOptionRows) {
    flagWidth = std::max(flagWidth, std::string(row.flags).size());
  }

  std::string text = "BomberBoy - two player bomb battle\n\nUsage: bomberboy [options]\n";
  for (const OptionRow& row : kOptionRows) {
    const std::size_t flagLength = std::string(row.flags).size();
    text += "  ";
    text += row.flags;
    text.append(flagWidth - flagLength + 2, ' ');
    text += row.description;
    text += '\n';
  }

  text +=
      "\nControls\n"
      "  Player 1   W A S D to move, Space to drop a bomb\n"
      "  Player 2   Arrow keys to move, Enter or Right Ctrl to drop a bomb\n"
      "  C  hand player two to the computer (or take it back)\n"
      "  R  restart round (or rematch once the match is over)\n"
      "  P  pause    F1  help    F2  screenshot    F3  FPS counter\n"
      "  F11 fullscreen    Esc  quit (closes the help panel first)\n";
  return text;
}

}  // namespace bb
