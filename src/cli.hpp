#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bb {

/// Largest frame cap `--fps` accepts; 0 means "no cap".
constexpr int kMaxFrameCap = 1000;
/// Upper end of the `--scale` range; the window is also clamped to the desktop.
constexpr int kMaxWindowScale = 4;
/// Longest scripted `--autoplay` run, in seconds.
constexpr float kMaxAutoplay = 600.f;

/// Everything the command line can change about a run.
struct Options {
  bool runSelfTest = false;
  bool cpuOpponent = false;
  bool fullscreen = false;
  bool hasSeed = false;
  std::uint32_t seed = 0;
  std::string screenshotPath;
  float autoplaySeconds = 0.f;
  int scale = 1;
  int frameCap = 60;
};

/// What came out of reading the arguments: the options, a request to print the
/// help and stop, or a message for the user.
struct ParsedOptions {
  Options options;
  bool ok = true;
  bool wantsHelp = false;
  std::string error;
};

/// Reads the arguments (without the program name). Unknown flags, flags whose
/// value is missing and values outside the documented range all fail with
/// `ok == false` and a ready to print `error`, so a typo never turns into a
/// silent default.
ParsedOptions parseOptions(const std::vector<std::string>& arguments);

inline ParsedOptions parseOptions(int argc, char** argv) {
  std::vector<std::string> arguments;
  arguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0);
  for (int i = 1; i < argc; ++i) {
    arguments.emplace_back(argv[i]);
  }
  return parseOptions(arguments);
}

/// The `--help` text, laid out from the same table the parser follows so the two
/// cannot drift apart.
std::string usageText();

}  // namespace bb
