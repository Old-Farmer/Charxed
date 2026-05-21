#include "status_line.h"

#include "buffer.h"
#include "cursor.h"
#include "draw.h"
#include "filetype.h"
#include "options.h"
#include "window.h"

namespace charxed {

StatusLine::StatusLine(Cursor* cursor, GlobalOpts* global_opts, Mode* mode)
    : cursor_(cursor), global_opts_(global_opts), mode_(mode) {}

void StatusLine::Draw() {
    ColorSchemeType t = kStatusLine;

    auto scheme = global_opts_->GetOpt<ColorScheme>(kOptColorScheme);

    Buffer* b;
    if (IsPeel(*mode_)) {
        b = cursor_->restore_from_peel->area_.buffer_;
    } else {
        b = cursor_->in_window->area_.buffer_;
    }
    left_str_ = fmt::format("{:<" CHX_MODE_WIDTH "} {}{}",
                            kModeString[static_cast<int>(*mode_)], b->Name(),
                            kBufferStateString[static_cast<int>(b->state())]);

    size_t drawn_width;
    DrawLine(*term_, left_str_, {0, 0}, 0, width_, row_, 0, nullptr, scheme,
             scheme[t], left_str_.size(), 0, false, true, drawn_width);

    int64_t line, character_in_line;
    if (IsPeel(*mode_)) {
        line = cursor_->restore_from_peel->area_.b_view_->cursor_state.pos.line;
        character_in_line = cursor_->restore_from_peel->area_.b_view_
                                ->cursor_state.character_in_line_;
    } else {
        line = cursor_->pos.line;
        character_in_line = cursor_->character_in_line;
    }

    right_str_ =
        fmt::format("  {},{}  {}  {}{}  {}", line + 1, character_in_line + 1,
                    FiletypeStrRep(b->filetype()),
                    b->opts().GetOpt<bool>(kOptTabSpace) ? "Sp" : "Tb",
                    b->opts().GetOpt<int64_t>(kOptTabStop), b->eol_seq());
    // all is ascii character, so str len == width
    term_->Print(width_ - right_str_.length(), row_, scheme[t],
                 right_str_.c_str());
}

}  // namespace charxed
