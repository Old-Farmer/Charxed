#pragma once

#include <cstddef>
#include <string_view>
constexpr const char* kDocsPath = "docs/";
constexpr const char* kHelpDoc = "help.md";
constexpr const char* kResourcePath = "resource/";
constexpr const char* kTSQueryPath = "resource/ts-queries/";

constexpr const char* kWSLEnv = "WSL_DISTRO_NAME";

constexpr const char* kIcon = "🥭";

constexpr size_t kMaxSizeTWidth = 20;

constexpr std::string_view kSmile = R"(
      _____
   .-'     '-.
  /  _   _    \
 |  (o) (o)    |
 |      ^      |
 |   \_____/   |
  \           /
   '-._____.-'
)";

constexpr const char* kStartup[] = {
    R"( __  __   ____   ___)",
    R"(|  \/  | / ___| / _ \)",
    R"(| |\/| || |  _ | | | |    MANGO EDITOR)",
    R"(| |  | || |_| || |_| |)",
    R"(|_|  |_| \____| \___/    BY SHIXIN CHAI)",
    "",
    "",
    R"(    tips:   :help<CR>   open docs)",
    R"(            :smile<CR>  :))",
    "",
    R"(      Press any key to continue...)"};

constexpr size_t kStartupWidth = 39;
constexpr size_t kStartupHeight = 11;
