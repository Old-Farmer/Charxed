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

#define CHX_BUFFER_STATE_TABLE        \
    X(kModified, "[M]")               \
    X(kNotModified, "")               \
    X(kCannotRead, "[Can't Read]")    \
    X(kHaveNotRead, "[Haven't Read]") \
    X(kReadOnly, "[RdOnly]")          \
    X(kCodingInvalid, "[CodingInvalid]")

enum class BufferState : int {
#define X(state, str) state,
    CHX_BUFFER_STATE_TABLE
#undef X
};

constexpr std::string_view kBufferStateString[] = {
#define X(state, str) str,
    CHX_BUFFER_STATE_TABLE
#undef X
};

// Determines the current operational context
enum class Context {
    kEditor,
    kExplorer,

    __kCount,
};

#define CHX_MODE_TABLE                                                   \
    X(kNormal, "NORMAL")                                                 \
    X(kInsert, "INSERT")                                                 \
    X(kSelect, "SELECT")                                                 \
    X(kSelectLine, "SELECT-L")                                           \
    X(kOperatorPending, "OP-PEND")                                       \
    X(kPeelCommand, "COMMAND") /* user is inputting sth */               \
    X(kPeelSearch, "SEARCH")   /* user is searching */                   \
    X(kPeelShow, "SHOW")       /* peel shows some multirow output and we \
                                              are in it.*/

// clang-format off
enum class Mode : int {
#define X(mode, str) mode,
    CHX_MODE_TABLE
#undef X
    _kCount,  // not mode, just for count
};
// clang-format on

constexpr std::string_view kModeString[] = {
#define X(mode, str) str,
    CHX_MODE_TABLE
#undef X
};

#define CHX_MODE_WIDTH "8"  // WIDTH for showing mode

inline bool IsPeel(Mode mode) {
    switch (mode) {
        case Mode::kPeelCommand:
            [[fallthrough]];
        case Mode::kPeelSearch:
            [[fallthrough]];
        case Mode::kPeelShow:
            return true;
        default:
            return false;
    }
}

inline bool InsertLike(Mode mode) {
    switch (mode) {
        case Mode::kInsert:
            [[fallthrough]];
        case Mode::kPeelCommand:
            [[fallthrough]];
        case Mode::kPeelSearch:
            return true;
        default:
            return false;
    }
}

#define CHX_SELECT_MODES Mode::kSelect, Mode::kSelectLine
#define CHX_DEFAULT_MODES Mode::kNormal, CHX_SELECT_MODES
#define CHX_ALL_MODES                                                       \
    Mode::kNormal, Mode::kInsert, CHX_SELECT_MODES, Mode::kOperatorPending, \
        Mode::kPeelShow, Mode::kPeelCommand, Mode::kPeelSearch

#define CHX_DEFAULT_CONTEXTS Context::kEditor
#define CHX_ALL_CONTEXTS Context::kEditor, Context::kExplorer

}  // namespace charxed
