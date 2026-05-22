#include "filetype.h"

#include <string>
#include <unordered_map>

#include "utils.h"

namespace charxed {

namespace {
constexpr FileType kDefaultFileType = FileType::kTxt;

// clang-format off
const std::unordered_map<zstring_view, FileType> kSuffixToFiletype = {
    {"c", FileType::kC},
    {"cpp", FileType::kCpp},
    {"cc", FileType::kCpp},
    {"cxx", FileType::kCpp},
    {"h", FileType::kCpp},
    {"hpp", FileType::kCpp},
    {"hh", FileType::kCpp},
    {"hxx", FileType::kCpp},
    {"rs", FileType::kRs},
    {"go", FileType::kGo},
    {"java", FileType::kJava},
    {"kt", FileType::kKt},
    {"cs", FileType::kCs},
    {"py", FileType::kPy},
    {"lua", FileType::kLua},
    {"js", FileType::kJs},
    {"ts", FileType::kTs},
    {"bash", FileType::kBash},
    {"sh", FileType::kSh},
    {"txt", kDefaultFileType},
    {"json", FileType::kJson},
    {"toml", FileType::kToml},
    {"yaml", FileType::kYaml},
    {"md", FileType::kMd},
    {"cmake", FileType::kCmake},
};

// file name is prior to suffix
const std::unordered_map<zstring_view, FileType> kNameToFiletype = {
    {"CMakeLists.txt", FileType::kCmake},
    {"Makefile", FileType::kMake},
};
// clang-format on

const std::unordered_map<zstring_view, FileType> kInnerStrRepToFileType = {
#define X(ft, inner, user) {#inner, FileType::ft},
    CHX_FILE_TYPE_TABLE
#undef X
};

}  // namespace

FileType DecideFiletype(std::string_view file_name) {
    auto iter = kNameToFiletype.find(file_name);
    if (iter != kNameToFiletype.end()) {
        return iter->second;
    }

    std::string::size_type pos = file_name.find_last_of('.');
    if (pos == std::string::npos) {
        return kDefaultFileType;
    }

    // TODO: regex and better
    std::string_view suffix(file_name.data() + pos + 1,
                            file_name.size() - pos - 1);
    auto iter2 = kSuffixToFiletype.find(suffix);
    if (iter2 == kSuffixToFiletype.end()) {
        return kDefaultFileType;
    }
    return iter2->second;
}

zstring_view FileTypesInnerStrRep(FileType filetype) {
    switch (filetype) {
#define X(ft, inner, user) \
    case FileType::ft:     \
        return #inner;
        CHX_FILE_TYPE_TABLE
#undef X
        default:
            CHX_ASSERT(false);
            return "";
    }
}

zstring_view FiletypeUserStrRep(FileType filetype) {
    switch (filetype) {
#define X(ft, inner, user) \
    case FileType::ft:     \
        return #user;
        CHX_FILE_TYPE_TABLE
#undef X
        default:
            CHX_ASSERT(false);
            return "";
    }
}

std::optional<FileType> InnerStrRepToFileType(std::string_view str) {
    auto iter = kInnerStrRepToFileType.find(str);
    if (iter == kInnerStrRepToFileType.end()) {
        return {};
    }
    return iter->second;
}

}  // namespace charxed
