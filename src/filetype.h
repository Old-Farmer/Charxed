#pragma once
#include <optional>
#include <string_view>

#include "utils.h"

namespace charxed {

#define CHX_FILE_TYPE_TABLE        \
    X(kNone, none, None)           \
    X(kC, c, C)                    \
    X(kCpp, cpp, C++)              \
    X(kRs, rust, Rust)             \
    X(kGo, go, Go)                 \
    X(kJava, java, Java)           \
    X(kKt, kotlin, Kotlin)         \
    X(kCs, csharp, C♯)             \
    X(kPy, python, Python)         \
    X(kLua, lua, Lua)              \
    X(kJs, javascript, Javascript) \
    X(kTs, typescript, Typescript) \
    X(kBash, bash, Bash)           \
    X(kSh, shell, Shell)           \
    X(kTxt, txt, Text)             \
    X(kJson, json, Json)           \
    X(kToml, toml, TOML)           \
    X(kYaml, yaml, YAML)           \
    X(kMd, markdown, Markdown)     \
    X(kCmake, cmake, CMake)        \
    X(kMake, make, Makefile)

// clang-format off
enum class FileType : int {
#define X(ft, ...) ft,
    CHX_FILE_TYPE_TABLE
#undef X
    __kCount
};
// clang-format on

FileType DecideFiletype(std::string_view file_name);

zstring_view FileTypesInnerStrRep(FileType filetype);

zstring_view FiletypeUserStrRep(FileType filetype);

std::optional<FileType> InnerStrRepToFileType(std::string_view str);

}  // namespace charxed
