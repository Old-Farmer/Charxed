#pragma once

#include <string_view>

namespace charxed {
enum class MouseState {
    kReleased,
    kLeftNotReleased,
    kRightNotReleased,
    kMiddleNotReleased,
    kLeftHolding,
};

enum class BufferState : int {
    kModified = 0,
    kNotModified = 1,
    kCannotRead = 2,
    kHaveNotRead = 3,
    kCannotCreate = 4,
    kReadOnly = 5,
    kCodingInvalid = 6,
};

constexpr std::string_view kBufferStateString[] = {
    "[M]",
    "",
    "[Can't Read]",
    "[Haven't Read]",
    "[Can't Create]",
    "[RdOnly]",
    "[CodingInvalid]",
};

enum class Mode : int {
    kNormal = 0,
    kInsert,
    kSelect,
    kSelectLine,
    kOperatorPending,
    kPeelCommand,  // user is inputting sth
    kPeelSearch,   // user is searching
    kPeelShow,     // peel shows some multirow output and we
                   // are in it.

    kNone,    // Used for some situations that don't care about mode.
    _kCount,  // not mode, just for count
};

constexpr std::string_view kModeString[] = {
    "NORMAL",  "INSERT", "SELECT", "SELECT-L", "OP-PEND",
    "COMMAND", "SEARCH", "SHOW",   "",         "",
};

#define CHX_VIM_MODE_WIDTH "8"  // WIDTH for showing mode

inline bool IsPeel(Mode mode) {
    return mode == Mode::kPeelCommand || mode == Mode::kPeelShow ||
           mode == Mode::kPeelSearch;
}

#define CHX_SELECT_MODES Mode::kSelect, Mode::kSelectLine
#define CHX_DEFAULT_MODES Mode::kNormal, CHX_SELECT_MODES
#define CHX_ALL_MODES                                                       \
    Mode::kNormal, Mode::kInsert, CHX_SELECT_MODES, Mode::kOperatorPending, \
        Mode::kPeelShow, Mode::kPeelCommand, Mode::kPeelSearch

}  // namespace charxed
