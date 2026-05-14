#include "window.h"

#include <gsl/util>

#include "buffer.h"
#include "buffer_manager.h"
#include "character.h"
#include "cursor.h"
#include "options.h"
#include "search.h"
#include "str.h"
#include "syntax.h"

namespace mango {
Window::Window(Cursor* cursor, GlobalOpts* global_opts, SyntaxParser* parser,
               ClipBoard* clipboard, BufferManager* buffer_manager) noexcept
    : cursor_(cursor),
      opts_(global_opts),
      parser_(parser),
      buffer_manager_(buffer_manager),
      area_(cursor, &opts_, parser, clipboard) {}

void Window::CursorGoLine(size_t line) {
    area_.b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (area_.CursorGoLineState(line, state)) {
        if (FarEnoughWithCursor(state)) {
            SetJumpPoint();
        }
        state.SetCursor(cursor_);
        area_.SelectionFollowCursor();
    }
}

Result Window::DeleteAtCursor() {
    if (area_.IsSelectionActive()) {
        return area_.DeleteSelection();
    }

    Buffer* buffer = area_.buffer_;
    Range range;
    if (cursor_->pos.byte_offset == 0) {  // first byte
        if (cursor_->pos.line == 0) {
            return kFail;
        }
        range = {{cursor_->pos.line - 1,
                  buffer->GetLineView(cursor_->pos.line - 1).Size()},
                 {cursor_->pos.line, 0}};
    } else if (GetOpt<bool>(kOptTabSpace)) {
        bool all_space = true;
        auto begin = buffer->Find({cursor_->pos.line, 0});
        for (auto iter = begin;
             iter.offset() - begin.offset() < cursor_->pos.byte_offset;
             iter.NextByte()) {
            if (iter.ThisByte() != kSpaceChar) {
                all_space = false;
                break;
            }
        }
        if (all_space) {
            range = {
                {cursor_->pos.line, (cursor_->pos.byte_offset - 1) / 4 * 4},
                cursor_->pos};
        } else {
            goto slow;
        }
    } else {
    slow:
        auto cur_line = area_.buffer_->GetLineView(cursor_->pos.line);
        Character character;
        auto iter = area_.buffer_->Find(cursor_->pos);
        auto prev = PrevCharacter(iter, cur_line.begin, character);

        char c = -1;
        character.Ascii(c);

        if (cur_line.Size() == cursor_->pos.byte_offset) {
            range = {
                {cursor_->pos.line, prev.offset() - cur_line.begin.offset()},
                cursor_->pos};
        } else {
            // may delete pairs
            NextCharacter(iter, cur_line.end, character);
            char this_char = -1;
            character.Ascii(this_char);
            bool need_delete_pairs =
                c != -1 && this_char != -1 && IsPair(c, this_char);
            range = {
                {cursor_->pos.line, prev.offset() - cur_line.begin.offset()},
                {cursor_->pos.line,
                 cursor_->pos.byte_offset + (need_delete_pairs ? 1 : 0)}};
        }
    }

    Pos pos;
    if (Result res; (res = buffer->Delete(range, nullptr, pos)) != kOk) {
        return res;
    }
    cursor_->pos = pos;
    cursor_->DontHoldColWant();
    parser_->ParseSyntaxAfterEdit(buffer);
    return kOk;
}

Result Window::AddStringAtCursor(std::string_view str, bool raw) {
    if (raw) {
        return area_.AddStringAtCursor(str);
    }

    // TODO: better support autopair autoindent when selection
    if (area_.IsSelectionActive()) {
        return area_.AddStringAtCursor(str);
    }

    char c = -1;
    if (str.size() == 1 && str[0] < CHAR_MAX && str[0] >= 0) {
        c = str[0];
    }

    if (c == -1) {
        return area_.AddStringAtCursor(str);
    }

    if (GetOpt<bool>(kOptAutoIndent) && c == '\n') {
        return TryAutoIndent(cursor_->pos);
    } else if (GetOpt<bool>(kOptAutoPair)) {
        if (IsPair(c)) {
            return TryAutoPair(str);
        } else {
            return area_.AddStringAtCursor(str);
        }
    }
    // Will not reach here.
    return kOk;
}

Result Window::NewLineAboveCursorline() {
    MGO_ASSERT(!area_.IsSelectionActive());
    if (cursor_->pos.line == 0) {
        return area_.AddStringAtPos({0, 0}, "\n", &cursor_->pos);
    }
    return TryAutoIndent(
        {cursor_->pos.line - 1,
         area_.buffer_->GetLineView(cursor_->pos.line - 1).Size()});
}

Result Window::NewLineUnderCursorline() {
    MGO_ASSERT(!area_.IsSelectionActive());
    return TryAutoIndent(
        {cursor_->pos.line,
         area_.buffer_->GetLineView(cursor_->pos.line).Size()});
}

Result Window::Replace(const Range& range, std::string_view str) {
    return area_.Replace(range, str);
}

Result Window::TryAutoPair(std::string_view str) {
    MGO_ASSERT(str.size() == 1 && str[0] < CHAR_MAX && str[0] >= 0);

    auto line = area_.buffer_->GetLineView(cursor_->pos.line);

    auto iter = area_.buffer_->Find(cursor_->pos);
    bool end_of_line = iter == line.end;
    char cur_c = end_of_line ? -1 : iter.ThisByte();  // -1 here just makes
                                                      // compiler happy, not
                                                      // used.
    bool start_of_line = cursor_->pos.byte_offset == 0;
    char prev_c;
    if (start_of_line) {
        iter.PrevByte();
        prev_c = iter.ThisByte();
    } else {
        prev_c = -1;
    }

    // Can we just move cursor next ?
    // e.g. (<cursor>) and input ')', we just move cursor right to ()<cursor>
    if (!start_of_line && !end_of_line) {
        if (IsPair(prev_c, cur_c) && cur_c == str[0]) {
            if (!area_.buffer_->IsLoad()) {
                return kBufferCannotLoad;
            }
            if (area_.buffer_->read_only()) {
                return kBufferReadOnly;
            }
            cursor_->pos.byte_offset++;
            cursor_->DontHoldColWant();
            return kOk;
        }
    }

    // Can't just move cursor next.
    // try auto pair
    auto [is_open, c_close] = IsPairOpen(str[0]);
    if (!is_open) {
        return area_.AddStringAtCursor(str);
    }

    if (end_of_line || !IsPair(str[0], cur_c)) {
        Pos pos = {cursor_->pos.line, cursor_->pos.byte_offset + 1};
        std::string pairs = std::string(str) + c_close;
        return area_.AddStringAtCursor(pairs, &pos);
    }

    return area_.AddStringAtCursor(str);
}

Result Window::TryAutoIndent(Pos pos) {
    MGO_ASSERT(!area_.IsSelectionActive());

    // TODO: use regex patterns for autoindent

    auto line = area_.buffer_->GetLine(pos.line);
    std::string indent = "";
    size_t cur_indent = 0;
    // Same as this line's indent
    size_t i = 0;
    auto tabstop = GetOpt<int64_t>(kOptTabStop);
    for (; i < line.size(); i++) {
        if (line[i] == kSpaceChar) {
            cur_indent++;
            indent.push_back(kSpaceChar);
        } else if (line[i] == '\t') {
            cur_indent = (cur_indent / tabstop + 1) * tabstop;
            indent.push_back('\t');
        } else {
            break;
        }
    }

    // When encounter some syntax blocks, We want one more indentation and sth.
    // else.
    // TODO: better language support
    Pos cursor_pos;
    bool maunally_set_cursor_pos = false;
    std::string str = "\n" + indent;
    auto tabspace = GetOpt<bool>(kOptTabSpace);
    // Try to check () {} [].
    // e.g.
    // {<cursor>}
    // ->
    // {
    // <indent><cursor>
    // }
    for (int64_t i = pos.byte_offset - 1; i >= 0; i--) {
        auto [is_open, want_right] = IsPairOpen(line[i]);
        if (!is_open) {
            if (line[i] == kSpaceChar || line[i] == '\t') {
                continue;
            }
            break;
        }

        if (tabspace) {
            int need_space = (cur_indent / tabstop + 1) * tabstop - cur_indent;
            str += std::string(need_space, kSpaceChar);
        } else {
            str += "\t";
        }
        for (i = pos.byte_offset; i < static_cast<int64_t>(line.size()); i++) {
            if (line[i] == want_right) {
                cursor_pos.line = pos.line + 1;
                cursor_pos.byte_offset = str.size() - 1;
                maunally_set_cursor_pos = true;
                str += "\n" + indent;
                break;
            } else if (line[i] != kSpaceChar && line[i] != '\t') {
                break;
            }
        }
        break;
    }

    if (maunally_set_cursor_pos) {
        return area_.AddStringAtPos(pos, str, &cursor_pos);
    } else {
        return area_.AddStringAtPos(pos, str);
    }
}

Result Window::GotoFile() {
    area_.b_view_->make_cursor_visible = true;

    TextTree::TextView path_view;
    std::string buf;
    std::string_view path;
    if (!area_.IsSelectionActive()) {
        path_view = FindPath(area_.buffer_->GetLineView(cursor_->pos.line),
                             area_.buffer_->Find(cursor_->pos));
    } else {
        path_view = area_.buffer_->GetContentView(
            area_.selection_->ToSelectRange(area_.buffer_));
    }
    if (path_view.Size() == 0) {
        return kError;
    }
    path = path_view.ToStringView(buf);

    std::string real_path;
    if (Path::IsAbsolutePath(path)) {
        real_path = path;
    } else {
        if (area_.buffer_->path().Empty()) {
            // cwd as fallback
            real_path = Path::GetCwd();
            // TODO: path normalization
            real_path.append(path);
        } else {
            real_path = area_.buffer_->path().Dir();
            // TODO: path normalization
            real_path.append(path);
        }
    }
    MGO_LOG_DEBUG("path: {}", real_path);
    try {
        // Check file stat to make sure that we don't create a buffer for
        // a completely wrong path.
        FileStat file_stat;
        Result res = GetFileStat(real_path, file_stat);
        if (res == kNotExist) {
            return kNotExist;
        }
        Buffer* buffer =
            buffer_manager_->AddBuffer(Buffer(opts_.global_opts_, real_path));
        AttachBuffer(buffer);
    } catch (FSException&) {
        return kError;
    }
    return kOk;
}

void Window::NextBuffer() {
    if (area_.buffer_->IsLastBuffer()) {
        return;
    }
    Buffer* next = area_.buffer_->next_;
    AttachBuffer(next);
}

void Window::PrevBuffer() {
    if (area_.buffer_->IsFirstBuffer()) {
        return;
    }
    Buffer* prev = area_.buffer_->prev_;
    AttachBuffer(prev);
}

void Window::AttachBuffer(Buffer* buffer) {
    DetachBuffer();
    MoveJumpHistoryCursorForwardAndTruncate();
    auto b_view_iter = buffer_views_.find(buffer->id());
    if (b_view_iter == buffer_views_.end()) {
        bool ok;
        std::tie(b_view_iter, ok) =
            buffer_views_.emplace(buffer->id(), BufferView());
        MGO_ASSERT(ok);
    }
    b_view_iter->second.RestoreCursorState(cursor_, buffer);
    area_.b_view_ = &b_view_iter->second;
    area_.buffer_ = buffer;
}

void Window::DetachBuffer() {
    if (area_.buffer_) {
        area_.b_view_->SaveCursorState(cursor_);
        InsertJumpHistory();
        area_.b_view_ = nullptr;
        area_.buffer_ = nullptr;
        area_.StopSelection();
    }
}

void Window::OnBufferDelete(const Buffer* buffer) {
    if (buffer == area_.buffer_) {
        if (buffer->IsFirstBuffer() && buffer->IsLastBuffer()) {
            AttachBuffer(buffer_manager_->AddBuffer({opts_.global_opts_}));
        } else if (buffer->IsFirstBuffer()) {
            NextBuffer();
        } else {
            PrevBuffer();
        }
    }
    buffer_views_.erase(buffer->id());
}

BufferSearchState Window::CursorGoSearchResult(bool next, size_t count,
                                               bool keep_current_if_one) {
    CursorState state(cursor_);
    BufferSearchState search_state = area_.CursorGoSearchResultState(
        b_search_context_, next, count, keep_current_if_one, state);
    if (search_state.total == 0) {
        return search_state;
    }
    SetJumpPoint();
    state.SetCursor(cursor_);
    return search_state;
}

void Window::InsertJumpHistory() {
    BufferView b_view = *area_.b_view_;
    b_view.SaveCursorState(cursor_);
    if (jump_history_cursor_ == jump_history_->end()) {
        jump_history_->emplace_back(b_view, area_.buffer_->id());
        jump_history_cursor_--;
        return;
    }
    jump_history_cursor_->b_view = b_view;
    jump_history_cursor_->buffer = area_.buffer_->id();
}

bool Window::FarEnoughWithCursor(const CursorState& state) {
    return (cursor_->pos.line > state.pos.line
                ? cursor_->pos.line - state.pos.line
                : state.pos.line - cursor_->pos.line) >= area_.height_ / 2;
}

void Window::MoveJumpHistoryCursorForwardAndTruncate() {
    if (jump_history_cursor_ != jump_history_->end()) {
        jump_history_cursor_ =
            jump_history_->erase(++jump_history_cursor_, jump_history_->end());
    }
    while (jump_history_->size() >
           static_cast<size_t>(GetOpt<int64_t>(kOptMaxJumpHistory))) {
        jump_history_->pop_front();
    }
}

void Window::SetJumpPoint() {
    InsertJumpHistory();
    MoveJumpHistoryCursorForwardAndTruncate();
}

void Window::JumpForward() {
    JumpHistory::iterator iter = jump_history_cursor_;
    if (iter == jump_history_->end() || ++iter == jump_history_->end()) {
        return;
    }
    while (iter != jump_history_->end()) {
        Buffer* b = buffer_manager_->FindBuffer(iter->buffer);
        if (b == nullptr) {
            iter = jump_history_->erase(iter);
            continue;
        }
        DetachBuffer();
        buffer_views_[b->id()] = iter->b_view;
        area_.b_view_ = &buffer_views_[b->id()];
        area_.buffer_ = b;
        area_.b_view_->RestoreCursorState(cursor_, b);
        jump_history_cursor_ = iter;
        return;
    }
}

void Window::JumpBackward() {
    JumpHistory::iterator iter = jump_history_cursor_;
    if (iter == jump_history_->begin()) {
        return;
    }
    while (iter != jump_history_->begin()) {
        iter--;
        MGO_ASSERT(iter != jump_history_->end());
        Buffer* b = buffer_manager_->FindBuffer(iter->buffer);
        if (b == nullptr) {
            iter = jump_history_->erase(iter);
            continue;
        }
        DetachBuffer();
        buffer_views_[b->id()] = iter->b_view;
        area_.b_view_ = &buffer_views_[b->id()];
        area_.buffer_ = b;
        area_.b_view_->RestoreCursorState(cursor_, area_.buffer_);
        jump_history_cursor_ = iter;
        return;
    }
}

int64_t Window::AllocId() noexcept { return cur_window_id_++; }

int64_t Window::cur_window_id_ = 0;

}  // namespace mango
