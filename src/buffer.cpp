#include "buffer.h"

#include <cstdio>
#include <cstring>
#include <gsl/util>

#include "cursor.h"
#include "editor_event_manager.h"
#include "exception.h"
#include "filetype.h"
#include "logging.h"
#include "options.h"

namespace charxed {

int64_t Buffer::cur_buffer_id_ = 0;
std::vector<bool> Buffer::new_file_alloced_ids_ = {};

Buffer::Buffer(GlobalOpts* global_opts, EditorEventManager* event_manager,
               bool new_file)
    : opts_(global_opts), event_manager_(event_manager) {
    if (new_file) {
        new_file_info_ = std::make_unique<NewFileInfo>();
        for (size_t i = 0; i < new_file_alloced_ids_.size(); i++) {
            if (!new_file_alloced_ids_[i]) {
                new_file_alloced_ids_[i] = true;
                new_file_info_->id = i + 1;
                new_file_info_->name =
                    "[new-file-" + std::to_string(new_file_info_->id) + "]";
                return;
            }
        }
        new_file_alloced_ids_.resize(new_file_alloced_ids_.size() + 1);
        new_file_alloced_ids_.back() = true;
        new_file_info_->id = new_file_alloced_ids_.size();
        new_file_info_->name =
            "[new-file-" + std::to_string(new_file_info_->id) + "]";
    }
}

Buffer::Buffer(GlobalOpts* global_opts, const std::string& path,
               EditorEventManager* event_manager, bool read_only)
    : path_(path),
      read_only_(read_only),
      opts_(global_opts),
      event_manager_(event_manager) {}

Buffer::Buffer(GlobalOpts* global_opts, const Path& path,
               EditorEventManager* event_manager, bool read_only)
    : path_(path),
      read_only_(read_only),
      opts_(global_opts),
      event_manager_(event_manager) {}

Buffer::~Buffer() {
    if (new_file_info_) {
        new_file_alloced_ids_[new_file_info_->id - 1] = false;
    }
    if (ts_tree_) {
        ts_tree_delete(ts_tree_);
    }
}

void Buffer::Load() {
    auto _ = gsl::finally([this] {
        // abnormal state, fast return
        if (state_ != BufferState::kHaveNotRead) {
            return;
        }
        state_ =
            read_only_ ? BufferState::kReadOnly : BufferState::kNotModified;
        opts_.InitAfterBufferLoad(this);
        if (GetOpt<bool>(kOptBasicWordCompletion) && IsLoad() && !read_only()) {
            basic_word_completer_ =
                std::make_unique<BufferBasicWordCompleter>(this);
            basic_word_completer_->Enable();
        }
    });

    try {
        if (path_.Empty()) {
            tree_.BulkLoad("");
            return;
        }

        filetype_ = DecideFiletype(path_.FileName());

        CHX_LOG_DEBUG("file path {}", path_.AbsolutePath());
        try {
            File f(path_.AbsolutePath(), "r+");
            tree_.BulkLoad(f, eol_seq_);
        } catch (FileNotExistException&) {
            // Not exist, we don't create it now. It's better to create
            // it when save it.
            tree_.BulkLoad("");
            return;
        } catch (FileAccessException&) {
            // try readonly
            File f(path_.AbsolutePath(), "r");
            tree_.BulkLoad(f, eol_seq_);
            read_only_ = true;
        }

#ifndef NDEBUG
        // when debug, leave files under app dir not be read only
#else
        if (read_only_ || path_.AbsolutePath().find(Path::GetAppRoot()) !=
                              std::string::npos) {
            read_only_ = true;
        }
#endif
        // TODO: check file stat for more op
    } catch (CodingException& e) {
        tree_.BulkLoad("");  // ensure init
        state_ = BufferState::kCodingInvalid;
        throw;
    } catch (Exception& e) {
        tree_.BulkLoad("");  // ensure init
        state_ = BufferState::kCannotRead;
        throw;
    }
}

void Buffer::Reload(Pos* cursor_pos, Pos& cursor_pos_hint) {
    try {
        File f(path_.AbsolutePath(), "r");
        CHX_LOG_DEBUG("reload file path {}", path_.AbsolutePath());

        // Maybe the file is not read only now?
        // TODO: check it

        std::string str = f.ReadAll(eol_seq_);
        if (!CheckUtf8Valid(str)) {
            state_ = BufferState::kCodingInvalid;
            throw CodingException("{}", "utf8 encoding error");
        }

        Range range = {{0, 0},
                       {LineCnt() - 1, GetLineView(LineCnt() - 1).Size()}};

        // TODO: diff content.
        // And use diff to adjust cursor pos

        bool origin_readonly = read_only_;
        read_only_ = false;
        state_ = BufferState::kNotModified;
        cursor_pos_hint = FixCursorPos(*cursor_pos, str);
        Replace(range, str, cursor_pos, true, cursor_pos_hint);
        if (origin_readonly) {
            state_ = BufferState::kReadOnly;
            read_only_ = origin_readonly;
        } else {
            state_ = BufferState::kNotModified;
        }

    } catch (CodingException& e) {
        tree_.BulkLoad("");  // ensure init
        state_ = BufferState::kCodingInvalid;
        throw;
    } catch (Exception& e) {
        tree_.BulkLoad("");  // ensure init
        state_ = BufferState::kCannotRead;
        throw;
    }
}

void Buffer::Clear() {
    state_ = BufferState::kNotModified;
    tree_.BulkLoad("");
    version_++;
}

Result Buffer::Save() {
    if (path_.Empty()) {
        return kBufferNoBackupFile;
    }

    if (!IsLoad()) {
        return kBufferCannotLoad;
    }

    if (read_only()) {
        return kBufferReadOnly;
    }

    if (event_manager_) {
        event_manager_->EmitEvent(EditorEvent::kBeforeBufferSave, this);
    }

    std::string swap_file_path;
    swap_file_path.reserve(path_.AbsolutePath().size() + kSwapSuffix.size());
    swap_file_path.append(path_.AbsolutePath());
    swap_file_path.append(kSwapSuffix);

    File swap_file = File(swap_file_path, "w");

    if (eol_seq_ == EOLSeq::kLF) {
        for (auto iter = tree_.BlockBegin(); iter != tree_.BlockEnd();
             iter.Next()) {
            auto str = iter.Data();
            size_t s = fwrite(str.data(), 1, str.size(), swap_file.file());
            if (s < str.size()) {
                throw IOException("fwrite error: {}", strerror(errno));
            }
        }
    } else {
        for (auto iter = tree_.BlockBegin(); iter != tree_.BlockEnd();
             iter.Next()) {
            auto str = iter.Data();
            size_t last = 0;
            for (size_t i = 0; i < str.size(); i++) {
                if (str[i] == '\n') {
                    size_t s = fwrite(str.data() + last, 1, i - last,
                                      swap_file.file());
                    if (s < i - last) {
                        throw IOException("fwrite error: {}", strerror(errno));
                    }
                    s = fwrite("\r\n", 1, 2, swap_file.file());
                    if (s < 2) {
                        throw IOException("fwrite error: {}", strerror(errno));
                    }
                    last = i + 1;
                }
            }
            if (last != str.size()) {
                size_t s = fwrite(str.data() + last, 1, str.size() - last,
                                  swap_file.file());
                if (s < str.size() - last) {
                    throw IOException("fwrite error: {}", strerror(errno));
                }
            }
        }
    }

    if (fflush(swap_file.file()) == EOF) {
        throw IOException("fflush error: {}", strerror(errno));
    }
    swap_file.Fsync();
    int ret = rename(swap_file_path.c_str(), path_.AbsolutePath().c_str());
    if (ret == -1) {
        throw IOException("rename error: {}", strerror(errno));
    }

    state_ = BufferState::kNotModified;
    return kOk;
}

Result Buffer::SaveAs(const Path& path) {
    CHX_ASSERT(!path.Empty());
    Path old_p = path_;
    path_ = path;

    // Allow write if the buffer was saved as another path.
    bool old_read_only = read_only_;
    if (old_p != path) {
        read_only_ = false;
    }

    try {
        Result res = Save();
        if (res != kOk) {
            path_ = old_p;
            read_only_ = old_read_only;
            return res;
        }
    } catch (IOException& e) {
        path_ = old_p;
        read_only_ = old_read_only;
        throw;
    }
    if (old_p.Empty()) {
        if (new_file_info_) {
            new_file_alloced_ids_[new_file_info_->id - 1] = false;
            new_file_info_.reset();
        }
    }
    read_only_ = old_read_only;

    // Reload ft stuff
    filetype_ = DecideFiletype(path_.FileName());
    opts_.InitAfterBufferLoad(this);
    return kOk;
}

void Buffer::Edit(const BufferEdit& edit, Pos& cursor_pos_hint) {
    switch (edit.GetType()) {
        case BufferEdit::kAdd:
            AddInner(edit.range.begin, edit.str, cursor_pos_hint, true);
            break;
        case BufferEdit::kDelete:
            DeleteInner(edit.range, cursor_pos_hint, false, true);
            break;
        case BufferEdit::kReplace:
            ReplaceInner(edit.range, edit.str, cursor_pos_hint, false);
            break;
    }
    if (ts_tree_) {
        ts_tree_edit(ts_tree_, &ts_edit_);
    }
}

void Buffer::AddInner(Pos pos, std::string_view str, Pos& cursor_pos_hint,
                      bool record_ts_edit) {
    auto iter = tree_.Find(pos);
    size_t offset = iter.offset();
    tree_.Add(iter, str);
    cursor_pos_hint = pos;
    for (char c : str) {
        if (c == '\n') {
            cursor_pos_hint.line++;
            cursor_pos_hint.byte_offset = 0;
        } else {
            cursor_pos_hint.byte_offset++;
        }
    }
    // CHX_LOG_DEBUG("del cursor line {}, byte_offset {}", cursor_pos_hint.line,
    //               cursor_pos_hint.byte_offset);
    Modified();
    if (record_ts_edit) {
        ts_edit_.start_point.row = pos.line;
        ts_edit_.start_point.column = pos.byte_offset;
        ts_edit_.old_end_point.row = pos.line;
        ts_edit_.old_end_point.column = pos.byte_offset;
        ts_edit_.new_end_point.row = cursor_pos_hint.line;
        ts_edit_.new_end_point.column = cursor_pos_hint.byte_offset;
        ts_edit_.start_byte = offset;
        ts_edit_.old_end_byte = offset;
        ts_edit_.new_end_byte = offset + str.size();
    }
}

std::string Buffer::DeleteInner(const Range& range, Pos& cursor_pos_hint,
                                bool record_reverse, bool record_ts_edit) {
    CHX_ASSERT(LineCnt() > range.end.line);
    CHX_ASSERT(range.begin.line < range.end.line ||
               (range.begin.line == range.end.line &&
                range.begin.byte_offset <= range.end.byte_offset));
    TextTree::TextView view;
    view.begin = tree_.Find(range.begin);
    view.end = tree_.Find(range.end);
    size_t offset = view.begin.offset();
    std::string old_str;
    size_t old_str_size = view.end.offset() - offset;
    if (record_reverse) {
        old_str = view.ToString();
    }
    tree_.Delete(view.begin, view.end);

    cursor_pos_hint = range.begin;
    // CHX_LOG_DEBUG("del cursor line {}, byte_offset {}", cursor_pos_hint.line,
    //               cursor_pos_hint.byte_offset);

    Modified();
    if (record_ts_edit) {
        ts_edit_.start_point.row = range.begin.line;
        ts_edit_.start_point.column = range.begin.byte_offset;
        ts_edit_.old_end_point.row = range.end.line;
        ts_edit_.old_end_point.column = range.end.byte_offset;
        ts_edit_.new_end_point.row = cursor_pos_hint.line;
        ts_edit_.new_end_point.column = cursor_pos_hint.byte_offset;
        ts_edit_.start_byte = offset;
        ts_edit_.old_end_byte = offset + old_str_size;
        ts_edit_.new_end_byte = offset;
    }

    return old_str;
}

std::string Buffer::ReplaceInner(const Range& range, std::string_view str,
                                 Pos& cursor_pos_hint, bool record_reverse) {
    Pos out_pos;
    std::string old_str = DeleteInner(range, out_pos, record_reverse, true);
    AddInner(out_pos, str, cursor_pos_hint, false);

    // Others record in DeleteInner
    ts_edit_.new_end_point.row = cursor_pos_hint.line;
    ts_edit_.new_end_point.column = cursor_pos_hint.byte_offset;
    ts_edit_.new_end_byte = ts_edit_.start_byte + str.size();

    return old_str;
}

bool Buffer::TryRecordMerge(const BufferEditHistoryItem& item) {
    CHX_ASSERT(edit_history_.Size() > 0);
    auto last_item_optional = edit_history_.GetItemJustBeforeCursor();
    if (!last_item_optional.has_value()) {
        return false;
    }
    BufferEditHistoryItem& last_item = *last_item_optional;
    if (last_item.origin.Size() != 1 || item.origin.Size() != 1) {
        return false;
    }

    auto& last_item_origin = *last_item.origin.Data();
    auto& last_item_reverse = *last_item.reverse.Data();
    auto& item_origin = *item.origin.Data();
    auto& item_reverse = *item.reverse.Data();
    if (last_item_origin.str.empty() && item_origin.str.empty() &&
        last_item_origin.range.begin == item_origin.range.end) {
        // adjacent deletes
        last_item_origin.range.begin = item_origin.range.begin;
        last_item.origin_pos_hint = item.origin_pos_hint;

        last_item_reverse.range.begin = item_reverse.range.begin;
        last_item_reverse.range.end = item_reverse.range.end;
        last_item_reverse.str.insert(0, item_reverse.str);
        return true;
    } else if (last_item_origin.range.begin == last_item_origin.range.end &&
               item_origin.range.begin == item_origin.range.end &&
               last_item_reverse.range.end == item_reverse.range.begin) {
        // adjacent adds
        last_item_reverse.range.end = item_reverse.range.end;

        last_item_origin.str.append(item_origin.str);
        last_item.origin_pos_hint = item.origin_pos_hint;
        return true;
    }
    // THINK IT: adjacent replaces need to be merged?
    return false;
}

void Buffer::Record(BufferEditHistoryItem&& item) {
    // No item in history, return fast
    if (edit_history_.Size() == 0) {
        edit_history_.PushItem(
            std::move(item),
            static_cast<size_t>(GetOpt<int64_t>(kOptMaxEditHistory)));
        return;
    }

    // Can we merge adjacent edits?
    if (TryRecordMerge(item)) {
        return;
    }

    if (edit_history_.PushItem(std::move(item),
                               static_cast<size_t>(GetOpt<int64_t>(
                                   kOptMaxEditHistory))) == kWrapHistory) {
        havent_wrap_history_ = true;
    }
}

Result Buffer::Add(Pos pos, std::string_view str, const Pos* cursor_pos,
                   bool use_given_pos_hint, Pos& cursor_pos_hint,
                   bool pos_hint_prefer_begin) {
    if (!IsLoad()) {
        return kBufferCannotLoad;
    }
    if (read_only()) {
        return kBufferReadOnly;
    }
    Pos origin_pos_hint;
    Pos initial_pos = cursor_pos ? *cursor_pos : pos;
    AddInner(pos, str, origin_pos_hint, true);
    if (ts_tree_) {
        ts_tree_edit(ts_tree_, &ts_edit_);
    }
    if (!use_given_pos_hint) {
        cursor_pos_hint = FixCursorPosAfterAdd(
            initial_pos, pos, origin_pos_hint, pos_hint_prefer_begin);
    }
    if (GetOpt<int64_t>(kOptMaxEditHistory) <= 0) {
        return kOk;
    }

    BufferEditHistoryItem item;
    item.origin.PushBack(BufferEdit::Add(pos, str));
    item.origin_pos_hint = cursor_pos_hint;

    item.reverse.PushBack(BufferEdit::Delete({pos, origin_pos_hint}));
    item.reverse_pos_hint = initial_pos;
    Record(std::move(item));
    return kOk;
}

Result Buffer::Delete(const Range& range, const Pos* cursor_pos,
                      bool use_given_pos_hint, Pos& cursor_pos_hint) {
    if (!IsLoad()) {
        return kBufferCannotLoad;
    }
    if (read_only()) {
        return kBufferReadOnly;
    }
    Pos origin_pos_hint;
    Pos initial_pos = cursor_pos ? *cursor_pos : range.end;
    std::string old_str = DeleteInner(range, origin_pos_hint, true, true);
    if (ts_tree_) {
        ts_tree_edit(ts_tree_, &ts_edit_);
    }
    if (!use_given_pos_hint) {
        cursor_pos_hint = FixCursorPosAfterDelete(initial_pos, range);
    }
    if (GetOpt<int64_t>(kOptMaxEditHistory) <= 0) {
        return kOk;
    }

    BufferEditHistoryItem item;
    item.origin.PushBack(BufferEdit::Delete(range));
    item.origin_pos_hint = cursor_pos_hint;

    item.reverse.PushBack(BufferEdit::Add(origin_pos_hint, std::move(old_str)));
    item.reverse_pos_hint = initial_pos;
    Record(std::move(item));
    return kOk;
}

Result Buffer::Replace(const Range& range, std::string_view str,
                       const Pos* cursor_pos, bool use_given_pos_hint,
                       Pos& cursor_pos_hint, bool pos_hint_prefer_begin) {
    if (!IsLoad()) {
        return kBufferCannotLoad;
    }
    if (read_only()) {
        return kBufferReadOnly;
    }

    Pos origin_pos_hint;
    Pos initial_pos = cursor_pos ? *cursor_pos : range.end;
    std::string old_str = ReplaceInner(range, str, origin_pos_hint, true);
    if (ts_tree_) {
        ts_tree_edit(ts_tree_, &ts_edit_);
    }
    if (!use_given_pos_hint) {
        cursor_pos_hint = FixCursorPosAfterReplace(
            initial_pos, range, origin_pos_hint, pos_hint_prefer_begin);
    }
    if (GetOpt<int64_t>(kOptMaxEditHistory) <= 0) {
        return kOk;
    }

    BufferEditHistoryItem item;
    item.origin.PushBack(BufferEdit::Replace(range, str));
    item.origin_pos_hint = cursor_pos_hint;

    item.reverse.PushBack(BufferEdit::Replace({range.begin, origin_pos_hint},
                                              std::move(old_str)));
    item.reverse_pos_hint = initial_pos;
    Record(std::move(item));
    return kOk;
}

Result Buffer::BatchEdit(const BufferEditBatch& edit_batch,
                         const Pos* cursor_pos, bool use_given_pos_hint,
                         Pos& cursor_pos_hint, bool pos_hint_prefer_begin) {
    if (!IsLoad()) {
        return kBufferCannotLoad;
    }
    if (read_only()) {
        return kBufferReadOnly;
    }

    int64_t max_edit_history = GetOpt<int64_t>(kOptMaxEditHistory);

    Pos origin_pos_hint;
    BufferEditHistoryItem item;
    std::string str;
    CHX_ASSERT(edit_batch.Size() > 0);
    Pos initial_pos = cursor_pos ? *cursor_pos : edit_batch.Data()[0].range.end;
    if (!use_given_pos_hint) {
        cursor_pos_hint = initial_pos;
    }
    for (int64_t i = edit_batch.Size() - 1; i >= 0; i--) {
        const auto& edit = edit_batch.Data()[i];
        switch (edit.GetType()) {
            case BufferEdit::kAdd:
                AddInner(edit.range.begin, edit.str, origin_pos_hint, true);
                if (max_edit_history > 1) {
                    item.reverse.PushBack(BufferEdit::Delete(
                        {edit.range.begin, origin_pos_hint}));
                }
                if (!use_given_pos_hint) {
                    cursor_pos_hint = FixCursorPosAfterAdd(
                        cursor_pos_hint, edit.range.begin, origin_pos_hint,
                        pos_hint_prefer_begin);
                }
                break;
            case BufferEdit::kDelete:
                str = DeleteInner(edit.range, origin_pos_hint, true, true);
                if (max_edit_history > 1) {
                    item.reverse.PushBack(
                        BufferEdit::Add(origin_pos_hint, str));
                }
                if (!use_given_pos_hint) {
                    cursor_pos_hint =
                        FixCursorPosAfterDelete(cursor_pos_hint, edit.range);
                }
                break;
            case BufferEdit::kReplace:
                str = ReplaceInner(edit.range, edit.str, origin_pos_hint, true);
                if (max_edit_history > 1) {
                    item.reverse.PushBack(BufferEdit::Replace(
                        {edit.range.begin, origin_pos_hint}, str));
                }
                if (!use_given_pos_hint) {
                    cursor_pos_hint = FixCursorPosAfterReplace(
                        cursor_pos_hint, edit.range, origin_pos_hint,
                        pos_hint_prefer_begin);
                }
                break;
        }
        if (ts_tree_) {
            ts_tree_edit(ts_tree_, &ts_edit_);
        }
    }
    if (max_edit_history > 1) {
        item.origin_pos_hint = cursor_pos_hint;
        item.reverse_pos_hint = initial_pos;
        // TODO: Not reverse it.
        item.origin = edit_batch;
        std::reverse(item.reverse.Data(),
                     item.reverse.Data() + edit_batch.Size());
        Record(std::move(item));
    }
    return kOk;
}

Result Buffer::Redo(Pos& cursor_pos_hint) {
    if (!edit_history_.MoveCursorForward()) {
        return kNoHistoryAvailable;
    }

    CHX_ASSERT(edit_history_.GetItemJustBeforeCursor().has_value());
    BufferEditHistoryItem& item = *edit_history_.GetItemJustBeforeCursor();
    for (int64_t i = item.origin.Size() - 1; i >= 0; i--) {
        Edit(item.origin.Data()[i], cursor_pos_hint);
    }
    cursor_pos_hint = item.origin_pos_hint;
    return kOk;
}

Result Buffer::Undo(Pos& cursor_pos_hint) {
    if (!edit_history_.MoveCursorBackward()) {
        return kNoHistoryAvailable;
    }

    CHX_ASSERT(edit_history_.GetItemAtCursor().has_value());
    BufferEditHistoryItem& item = *edit_history_.GetItemAtCursor();
    size_t sz = item.reverse.Size();
    for (size_t i = 0; i < sz; i++) {
        Edit(item.reverse.Data()[i], cursor_pos_hint);
    }
    cursor_pos_hint = item.reverse_pos_hint;
    if (edit_history_.IsCursorBegin() && havent_wrap_history_ &&
        state_ == BufferState::kModified) {
        state_ = BufferState::kNotModified;
    }
    return kOk;
}

TSInputEdit Buffer::GetEditForTreeSitter() { return ts_edit_; }

void Buffer::AppendToList(Buffer* tail) noexcept {
    tail->prev_->next_ = this;
    prev_ = tail->prev_;
    next_ = tail;
    tail->prev_ = this;
}

void Buffer::RemoveFromList() noexcept {
    // Must have a dummy head and tail_
    next_->prev_ = prev_;
    prev_->next_ = next_;
    next_ = nullptr;
    prev_ = nullptr;
}

bool Buffer::IsLastBuffer() const { return next_->next_ == nullptr; }
bool Buffer::IsFirstBuffer() const { return prev_->prev_ == nullptr; }

void Buffer::Modified() {
    CHX_ASSERT(IsLoad() && !read_only());
    state_ = BufferState::kModified;
    version_++;
}

}  // namespace charxed
