#include "options.h"

#include <unordered_set>

#include "buffer.h"
#include "file.h"
#include "filetype.h"
#include "fs.h"

namespace charxed {

static constexpr const char* kDefaultConfigPath = "resource/config/config.json";
static constexpr const char* kDefaultColorschemePath =
    "resource/config/colorscheme.json";
static const std::string kUserConfigPath =
    std::string(Path::GetConfig()) + "charxed/config.json";
static const std::string kUserColorschemePath =
    std::string(Path::GetConfig()) + "charxed/colorscheme.json";

static const std::unordered_map<std::string_view, OptKey> kStrRepToOptKey{
#define X(t, str, ...) {#str, t},
    CHX_BUFFER_OPT_TABLE CHX_WINDOW_OPT_TABLE CHX_GLOBAL_OPT_TABLE
#undef X
};

static std::unordered_map<std::string_view, ColorSchemeType>
    kStrToColorSchemeType{
#define X(t, str) {#str, t},
        CHX_COLOR_SCHEME_TABLE
#undef X
    };

static const std::unordered_map<std::string_view, Terminal::Color>
    kBasedColors = {
        {"default", Terminal::kDefault}, {"black", Terminal::kBlack},
        {"red", Terminal::kRed},         {"green", Terminal::kGreen},
        {"yellow", Terminal::kYellow},   {"blue", Terminal::kBlue},
        {"magenta", Terminal::kMagenta}, {"cyan", Terminal::kCyan},
        {"white", Terminal::kWhite},
};
static const std::unordered_map<std::string_view, Terminal::Effect> kEffects = {
    {"bold", Terminal::kBold},
    {"underline", Terminal::kUnderline},
    {"reverse", Terminal::kReverse},
    {"italic", Terminal::kItalic},
    {"blink", Terminal::kBlink},
    {"brighter", Terminal::kBright},
    {"dim", Terminal::kDim},
    {"strikeout", Terminal::kStrikeOut},
    {"underline2", Terminal::kUnderline2},
    {"overline", Terminal::kOverline},
    {"invisible", Terminal::kInvisible},
};

static const std::unordered_set<ColorSchemeType>
    kColorSchemeTypeFgBgMustAllExist = {
        kNormal,
        kStatusLine,
        kMenu,
        kSidebar,
};

const Terminal::Attr kTruecolorBegin = 0x000000;
const Terminal::Attr kTruecolorEnd = 0xFFFFFF;

static void GetColorScheme(bool truecolor, const Json& colorscheme_json,
                           ColorSchemeElement* colorscheme) {
    // Every colorscheme type should have fg and bg.
    // Fg and bg should have a color and >=0 effects.
    // We ignore unknown keys and values.
    int colorscheme_type_cnt = 0;
    for (const auto& [k, v] : colorscheme_json.items()) {
        const auto iter = kStrToColorSchemeType.find(k);
        if (iter == kStrToColorSchemeType.end()) {
            continue;
        }
        colorscheme_type_cnt++;

        ColorSchemeType type = iter->second;

        std::string_view attr_locs[2] = {"fg", "bg"};
        for (std::string_view attr_loc : attr_locs) {
            auto iter_attr = v.find(attr_loc);
            if (iter_attr == v.end()) {
                // allow don't have fg or bg in some colorscheme type
                continue;
            }

            if (!iter_attr->is_array()) {
                throw OptionLoadException(
                    "Option colorscheme{}/{}/{} is not "
                    "array",
                    truecolor ? "_truecolor" : "", k, attr_loc);
            }

            int colors_cnt = 0;
            for (const auto& attr : iter_attr.value()) {
                const std::string& str = attr.get_ref<const std::string&>();
                if (kEffects.find(str) != kEffects.end()) {
                    if (attr_loc == "fg") {
                        colorscheme[type].fg |= kEffects.find(str)->second;
                    } else {
                        colorscheme[type].bg |= kEffects.find(str)->second;
                    }
                } else if (kBasedColors.find(str) != kBasedColors.end() &&
                           !truecolor) {
                    if (attr_loc == "fg") {
                        colorscheme[type].fg |= kBasedColors.find(str)->second;
                    } else {
                        colorscheme[type].bg |= kBasedColors.find(str)->second;
                    }
                    colors_cnt++;
                } else if (truecolor) {
                    // skip #
                    Terminal::Attr color =
                        strtoll(str.c_str() + 1, nullptr, 16);
                    if (color >= kTruecolorBegin && color <= kTruecolorEnd) {
                        colors_cnt++;
                        if (color ==
                            kTruecolorBegin) {  // See Termbox2 tb_set_output
                            color = Terminal::kHiBlack;
                        }
                        if (attr_loc == "fg") {
                            colorscheme[type].fg |= color;
                        } else {
                            colorscheme[type].bg |= color;
                        }
                    }
                }
            }
            if (colors_cnt != 1) {
                throw OptionLoadException(
                    "{}", "In colorscheme, Color cnt wrong, expect one");
            }
            if (attr_loc == "fg") {
                colorscheme[type].fg_exist = true;
            } else {
                colorscheme[type].bg_exist = true;
            }
        }

        if (kColorSchemeTypeFgBgMustAllExist.find(type) !=
            kColorSchemeTypeFgBgMustAllExist.end()) {
            if (!colorscheme[type].bg_exist || !colorscheme[type].fg_exist) {
                throw OptionLoadException(
                    "Option colorscheme{}/{} must both has \"bg\" and \"fg\" "
                    "color",
                    truecolor ? "_truecolor" : "", k);
            }
        }
    }

    colorscheme[kNormalFg] = colorscheme[kNormal];
    colorscheme[kNormalFg].bg_exist = false;
    colorscheme_type_cnt++;

    if (colorscheme_type_cnt != __kColorSchemeTypeCount) {
        throw OptionLoadException("{}", "Colorscheme type cnt wrong");
    }
}

OptInfo GlobalOpts::GetOptInfo(OptKey key) {
    switch (key) {
#define X(k, str, type) \
    case k:             \
        return {OptScope::kBuffer, Type::type};
        CHX_BUFFER_OPT_TABLE
#undef X
#define X(k, str, type) \
    case k:             \
        return {OptScope::kWindow, Type::type};
        CHX_WINDOW_OPT_TABLE
#undef X
#define X(k, str, type) \
    case k:             \
        return {OptScope::kGlobal, Type::type};
        CHX_GLOBAL_OPT_TABLE
#undef X
        default:
            CHX_ASSERT(false);
            return {};
    }
}

void GlobalOpts::TryApply(const Json& config, const Json& colorscheme_config) {
    for (const auto& [k, v] : config.items()) {
        auto filetype = InnerStrRepToFileType(k);
        if (filetype.has_value()) {
            for (const auto& [inner_k, inner_v] : v.items()) {
                auto iter = kStrRepToOptKey.find(inner_k);
                if (iter == kStrRepToOptKey.end()) {
                    continue;
                }
                OptKey opt_key = iter->second;
                const OptInfo& opt_info = GetOptInfo(opt_key);
                if (opt_info.scope != OptScope::kBuffer) {
                    continue;
                }

                if (opt_info.type == Type::kBool && inner_v.is_boolean()) {
                    filetype_opts_[static_cast<int>(*filetype)][opt_key] =
                        reinterpret_cast<void*>(inner_v.get<bool>());
                } else if (opt_info.type == Type::kInteger &&
                           inner_v.is_number_integer()) {
                    filetype_opts_[static_cast<int>(*filetype)][opt_key] =
                        reinterpret_cast<void*>(inner_v.get<int64_t>());
                } else {
                    throw OptionLoadException(
                        "value type wrong: key: {}, v type: {} {}",
                        "/" + k + "/" + inner_k,
                        static_cast<int>(opt_info.type),
                        static_cast<int>(
                            inner_v.type()));  // flatten rep of key
                }
            }
            continue;
        }

        auto iter = kStrRepToOptKey.find(k);
        if (iter == kStrRepToOptKey.end()) {
            continue;
        }
        OptKey opt_key = iter->second;
        const OptInfo& opt_info = GetOptInfo(opt_key);
        if (opt_info.type == Type::kBool && v.is_boolean()) {
            opts_[opt_key] = reinterpret_cast<void*>(v.get<bool>());
        } else if (opt_info.type == Type::kInteger && v.is_number_integer()) {
            opts_[opt_key] = reinterpret_cast<void*>(v.get<int64_t>());
        } else {
            throw OptionLoadException("value type wrong: key {}", k);
        }
    }

    // Colorscheme
    const Json* colorscheme_json = nullptr;
    bool truecolor = GetOpt<bool>(kOptTrueColor);
    std::string colorscheme_str = config.at("colorscheme");
    if (colorscheme_str != "default") {
        colorscheme_json = &colorscheme_config.at(colorscheme_str);
    } else {
        colorscheme_json = &colorscheme_config.at(
            std::string("default") + (truecolor ? "_truecolor" : "8"));
    }

    auto colorscheme = new ColorSchemeElement[__kColorSchemeTypeCount];
    bzero(colorscheme, sizeof(ColorSchemeElement) *
                           __kColorSchemeTypeCount);  // For attr bit wise or
    GetColorScheme(truecolor, *colorscheme_json, colorscheme);
    opts_[kOptColorScheme] = colorscheme;
}

// We try to first load users config and merge with default.
// If any exception is throwed, then we just use our default config.
// We don't wrap our default config loading in catch.
// If errs occur on out default config, just let it crash.
void GlobalOpts::LoadConfig() {
    EOLSeq eol_seq;
    std::string default_config_str =
        File(std::string(Path::GetAppRoot() + kDefaultConfigPath), "r", false)
            .ReadAll(eol_seq);
    std::string default_colorscheme_str =
        File(std::string(Path::GetAppRoot() + kDefaultColorschemePath), "r",
             false)
            .ReadAll(eol_seq);

    try {
        std::string user_config_str;
        if (File::FileReadable(kUserConfigPath)) {
            user_config_str =
                File(kUserConfigPath, "r", false).ReadAll(eol_seq);
        }
        std::string user_colorscheme_str;
        if (File::FileReadable(kUserColorschemePath)) {
            user_colorscheme_str =
                File(kUserColorschemePath, "r", false).ReadAll(eol_seq);
        }

        // We merge with user config
        Json config = Json::parse(default_config_str);
        Json colorscheme = Json::parse(default_colorscheme_str);
        if (!user_config_str.empty()) {
            Json user_config = Json::parse(user_config_str);
            config.update(user_config, true);
        }
        if (!user_colorscheme_str.empty()) {
            Json user_colorscheme = Json::parse(user_colorscheme_str);
            colorscheme.update(user_colorscheme, true);
        }
        TryApply(config, colorscheme);
        user_config_valid_ = true;
        return;
    } catch (Exception& e) {
        user_config_valid_ = false;
        user_config_error_reason_ = e.what();
    } catch (Json::exception& e) {
        user_config_valid_ = false;
        user_config_error_reason_ = e.what();
    }

    Json config = Json::parse(default_config_str);
    Json colorscheme = Json::parse(default_colorscheme_str);
    TryApply(config, colorscheme);
}

GlobalOpts::GlobalOpts() { LoadConfig(); }

GlobalOpts::~GlobalOpts() {
    delete[] reinterpret_cast<ColorScheme*>(opts_[kOptColorScheme]);
}

Opts::Opts(GlobalOpts* global_options) : global_opts_(global_options) {}

void Opts::InitAfterBufferLoad(const Buffer* buffer) {
    opts_ = global_opts_->filetype_opts_[static_cast<int>(buffer->filetype())];
}

}  // namespace charxed
