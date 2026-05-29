#include "editor.h"

#include "character.h"
#include "clipboard.h"
#include "constants.h"
#include "fs.h"
#include "inttypes.h"  // IWYU pragma: keep
#include "options.h"
#include "subprocess.h"
#include "term.h"
#include "version.h"

// TODO: show sth. to users if modify the readonly buffer.

namespace charxed {

namespace {
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
    R"(  ______ _    _ __   __)",
    R"( / ____/| |  | |\ \ / /)",
    R"(| |     | |__| | \ V /      CHARXED)",
    R"(| |     |  __  |  | |)",
    R"(| |____ | |  | | / ^ \   BY SHIXIN CHAI)",
    R"( \_____||_|  |_|/_/ \_\)",
    "",
    "",
    R"(tips:   :help [doc]<ENTER>   open docs)",
    R"(        :edit <file><ENTER>  edit files)",
    R"(        :quit<ENTER>         quit)",
    R"(        :smile<ENTER>        :))",
    "",
    R"(      Press any key to continue...)",
};

constexpr const char* kVersionInfo =
    "Version: " CHX_VERSION "\nCommit: " CHX_COMMIT;

constexpr size_t kStartupWidth = 39;
constexpr size_t kStartupHeight = std::size(kStartup);

constexpr int kScreenMinWidth = 10;
constexpr int kScreenMinHeight = 3;
}  // namespace

void Editor::Init(std::unique_ptr<GlobalOpts> global_opts,
                  std::unique_ptr<InitOpts> init_opts) {
    global_opts_ = std::move(global_opts);

    loop_ = std::make_unique<EventLoop>(global_opts_.get());

    term_.Init(global_opts_.get());

    buffer_manager_ = std::make_unique<BufferManager>(&editor_event_manager_);
    clipboard_ = ClipBoard::CreateClipBoard(true);

    // Component
    status_line_ = std::make_unique<StatusLine>(&cursor_, global_opts_.get(),
                                                &mode_, &context_);
    peel_ = std::make_unique<MangoPeel>(&cursor_, global_opts_.get(),
                                        clipboard_.get(), buffer_manager_.get(),
                                        &command_manager_);
    cmp_menu_ = std::make_unique<CmpMenu>(&cursor_, global_opts_.get());
    explorer_ = std::make_unique<Explorer>(global_opts_.get(), &cursor_,
                                           &context_, buffer_manager_.get());

    syntax_parser_ = std::make_unique<SyntaxParser>(global_opts_.get());
    buffer_monitor_ = std::make_unique<BufferFSMonitor>(
        buffer_manager_.get(), &cursor_, syntax_parser_.get(), &mode_,
        &context_);

    // Create all buffers
    for (const char* path : init_opts->begin_files) {
        auto b = buffer_manager_->AddBuffer(
            Buffer(global_opts_.get(), std::string(path)));
        try {
            buffer_monitor_->MonitorBuffer(b);
        } catch (OSException& e) {
            NotifyUser(fmt::format("Monitor buffer error: {}", e.what()));
        }
    }

    // Create the first window.
    // If no buffer then create one no file backup buffer.
    Buffer* buf;
    if (buffer_manager_->Begin() != nullptr) {
        buf = buffer_manager_->Begin();
    } else {
        buf = buffer_manager_->AddBuffer({global_opts_.get()});
    }
    // CHX_LOG_DEBUG("buffer {}", buf->Name());
    text_window_ = std::make_unique<TextWindow>(
        &cursor_, global_opts_.get(), syntax_parser_.get(), clipboard_.get(),
        buffer_manager_.get());

    // Set Cursor in the first window
    cursor_.t_win = text_window_.get();
    cursor_.focused = cursor_.t_win;
    text_window_->AttachBuffer(buf);

    // Layout
    layout_manager_ = std::make_unique<LayoutManager>(
        text_window_.get(), status_line_.get(), peel_.get(), explorer_.get(),
        &context_);
    layout_manager_->ArrangeLayout();
    explorer_->Init(layout_manager_.get());

    // Keymaps & commands
    term_.SetCursorStyle(Terminal::CursorStyle::kBlock);
    InitKeymaps();
    InitCommands();

    if (!global_opts_->IsUserConfigValid()) {
        NotifyUser(
            "Error in your config file! Default config loaded. Please "
            "check your config." +
            global_opts_->GetUserConfigErrorReportStrAndReleaseIt());
    }

    RegisterEditorEventHandlers();
}

void Editor::RegisterEditorEventHandlers() {
    editor_event_manager_.AddHandler(
        EditorEvent::kBufferRemoved, [this](void* arg) {
            auto buffer = reinterpret_cast<Buffer*>(arg);
            if (!buffer->path().Empty()) {
                buffer_monitor_->UnmonitorBuffer(buffer);
            }
            text_window_->OnBufferDelete(buffer);
            syntax_parser_->OnBufferDelete(buffer);
        });
    editor_event_manager_.AddHandler(EditorEvent::kEditCharEdit,
                                     [this](void* arg) {
                                         (void)arg;
                                         StartAutoCompletionTimer();
                                     });
    editor_event_manager_.AddHandler(EditorEvent::kCommandCharEdit,
                                     [this](void* arg) {
                                         (void)arg;
                                         StartAutoCompletionTimer();
                                     });
    editor_event_manager_.AddHandler(EditorEvent::kSearchCharEdit,
                                     [this](void* arg) {
                                         (void)arg;
                                         StartSearchOnTypeTimer();
                                     });
}

void Editor::Loop() {
    bool in_bracketed_paste = false;
    std::string bracketed_paste_buffer;

    auto before_poll = [this] {
        if (!need_redraw_) {
            need_redraw_ = true;
            return;
        }

        // When the screen is too small, rendering is hard to cope with,
        // so we just don't do anything, swallow all events except resize
        // until the screen is bigger again.
        while (term_.Height() < kScreenMinHeight ||
               term_.Width() < kScreenMinWidth) {
            if (term_.Poll(-1)) {
                if (term_.WhatEvent() == Terminal::EventType::kResize) {
                    HandleResize();
                }
            }
        }

        PreProcess();
        Draw();
    };

    auto term_handler = [this, &in_bracketed_paste,
                         &bracketed_paste_buffer](Event e) {
        (void)e;
        CHX_ASSERT(e & kEventRead);
        if (e & (kEventClose | kEventError)) {
            throw TermException("{}", "Poll event close or event error");
        }

        // We use a while to eat all events.
        // Usually, there will be only one event.
        // Bracketed Paste and multi-codepoint grapheme will lead to mult
        // events; Otherwise, multi events means we meet a slow machine, bad
        // network or a bad routine executes too long. I think we should handle
        // it.
        while (term_.Poll(0)) {
            show_cmp_menu_ = false;
            if (autocmp_trigger_timer_ &&
                autocmp_trigger_timer_->IsTimingOn()) {
                loop_->timer_manager_.StopTimer(autocmp_trigger_timer_.get());
                CHX_ASSERT(!autocmp_trigger_timer_->IsTimingOn());
            }

            multirow_peel_keep_ = false;

            // Handle it and do sth
            switch (term_.WhatEvent()) {
                case Terminal::EventType::kKey: {
                    if (in_bracketed_paste) {
                        HandleBracketedPaste(bracketed_paste_buffer);
                    } else {
                        HandleKey();
                    }
                    break;
                }
                case Terminal::EventType::kMouse: {
                    HandleMouse();
                    break;
                }
                case Terminal::EventType::kResize: {
                    HandleResize();
                    break;
                }
                case Terminal::EventType::kBracketedPasteOpen: {
                    in_bracketed_paste = true;
                    break;
                }
                case Terminal::EventType::kBracketedPasteClose: {
                    in_bracketed_paste = false;
                    if (IsPeel(mode_)) {
                        peel_->AddStringAtCursor(
                            std::move(bracketed_paste_buffer));
                        layout_manager_->ArrangeLayout();
                    } else {
                        cursor_.t_win->AddStringAtCursor(
                            std::move(bracketed_paste_buffer));
                    }
                    bracketed_paste_buffer = "";
                    break;
                }
            }
        }

        // If autocmp trigger timer has started,
        // don't cancel it to avoid flash of cmp menu.
        if (!show_cmp_menu_ &&
            (autocmp_trigger_timer_ && !autocmp_trigger_timer_->IsTimingOn())) {
            CancellCompletion();
        }

        // Hide peel multi row content if user don't in the peel.
        if (!multirow_peel_keep_ && peel_->area_.height_ >= 2 &&
            !IsPeel(mode_)) {
            std::string output_hidden_hint =
                fmt::format("[Collapse {} rows...]", peel_->area_.height_);
            if (output_hidden_hint.size() > term_.Width()) {
                output_hidden_hint.resize(term_.Width());
            }
            NotifyUser(output_hidden_hint);
        }
    };

    auto buffer_fs_event_handler = [this](Event e) {
        if (e & (kEventClose | kEventError)) {
            throw FSException("{}", "Poll event close or event error");
        }

        CHX_ASSERT(e & kEventRead);
        if (!buffer_monitor_->HandleFsEvents()) {
            need_redraw_ = false;
        }
    };

    EventInfo term_tty;
    EventInfo term_resize;
    EventInfo buffer_fs;
    term_.GetFDs(term_tty.fd, term_resize.fd);
    buffer_fs.fd = buffer_monitor_->fd();
    term_tty.Interesting_events |= kEventRead;
    term_resize.Interesting_events |= kEventRead;
    buffer_fs.Interesting_events |= kEventRead;
    term_tty.handler = term_handler;
    term_resize.handler = std::move(term_handler);
    buffer_fs.handler = std::move(buffer_fs_event_handler);

    loop_->BeforePoll(std::move(before_poll));
    loop_->AddEventHandler(std::move(term_tty));
    loop_->AddEventHandler(std::move(term_resize));
    loop_->AddEventHandler(std::move(buffer_fs));

    loop_->Loop();
}

#define CHX_KEYMAP keymap_manager_.AddKeyseq

void Editor::InitKeymaps() {
    // Navigation
    CHX_KEYMAP("h", {[this] { cursor_.t_win->CursorGoLeft(Count()); }});
    CHX_KEYMAP("h", {[this] { peel_->CursorGoLeft(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("l", {[this] { cursor_.t_win->CursorGoRight(Count()); }},
               {CHX_DEFAULT_MODES});
    CHX_KEYMAP("l", {[this] { peel_->CursorGoRight(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("b", {[this] { cursor_.t_win->CursorGoWordBegin(Count()); }},
               {CHX_DEFAULT_MODES});
    CHX_KEYMAP("b", {[this] { peel_->CursorGoPrevWordBegin(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("e", {[this] { cursor_.t_win->CursorGoNextWordEnd(Count()); }},
               {CHX_DEFAULT_MODES});
    CHX_KEYMAP("e", {[this] { peel_->CursorGoNextWordEnd(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("w", {[this] { cursor_.t_win->CursorGoNextWordBegin(Count()); }},
               {CHX_DEFAULT_MODES});
    CHX_KEYMAP("w", {[this] { peel_->CursorGoNextWordBegin(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("k", {[this] { cursor_.focused->CursorGoUp(Count()); }},
               {CHX_DEFAULT_MODES}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("k", {[this] { peel_->CursorGoUp(Count()); }}, {Mode::kPeelShow},
               {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("j", {[this] { cursor_.focused->CursorGoDown(Count()); }},
               {CHX_DEFAULT_MODES}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("j", {[this] { peel_->CursorGoDown(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("0", {[this] { cursor_.t_win->CursorGoHome(); }});
    CHX_KEYMAP("0", {[this] { peel_->CursorGoHome(); }}, {Mode::kPeelShow},
               {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("$", {[this] { cursor_.t_win->CursorGoEnd(); }});
    CHX_KEYMAP("$", {[this] { peel_->CursorGoEnd(); }}, {Mode::kPeelShow},
               {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-f>",
               {[this] { cursor_.focused->CursorGoPageDown(Count()); }},
               {CHX_DEFAULT_MODES});
    CHX_KEYMAP("<c-f>", {[this] { peel_->CursorGoPageDown(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-b>", {[this] { cursor_.focused->CursorGoPageUp(Count()); }},
               {CHX_DEFAULT_MODES}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-b>", {[this] { peel_->CursorGoPageUp(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-d>",
               {[this] { cursor_.focused->CursorGoHalfPageDown(Count()); }},
               {CHX_DEFAULT_MODES}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-d>", {[this] { peel_->CursorGoHalfPageDown(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-u>",
               {[this] { cursor_.focused->CursorGoHalfPageUp(Count()); }},
               {CHX_DEFAULT_MODES}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-u>", {[this] { peel_->CursorGoHalfPageUp(Count()); }},
               {Mode::kPeelShow}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-o>", {[this] { cursor_.t_win->JumpBackward(); }},
               {Mode::kNormal});
    CHX_KEYMAP("<c-i>", {[this] { cursor_.t_win->JumpForward(); }},
               {Mode::kNormal});
    CHX_KEYMAP("G", {[this] {
                   size_t l;
                   if (count_ == 0) {
                       l = cursor_.t_win->area_.buffer_->LineCnt() - 1;
                   } else {
                       l = Count();
                   }
                   cursor_.t_win->CursorGoLine(l);
               }});
    CHX_KEYMAP("gg", {[this] { cursor_.t_win->CursorGoLine(0); }});
    CHX_KEYMAP("gf", {[this] { cursor_.t_win->GotoFile(); }});
    CHX_KEYMAP("f<any-cp>", {[this] {
                   find_forward_ = true;
                   // For simplicity, currently we only find one codepoint.
                   c_to_find_.Set(term_.EventKeyInfo().codepoint);
                   cursor_.t_win->FindNextCharacterAndCursorGoInCurrentLine(
                       c_to_find_);
               }});
    CHX_KEYMAP("F<any-cp>", {[this] {
                   find_forward_ = false;
                   c_to_find_.Set(term_.EventKeyInfo().codepoint);
                   cursor_.t_win->FindPrevCharacterAndCursorGoInCurrentLine(
                       c_to_find_);
               }});
    CHX_KEYMAP(";", {[this] {
                   if (find_forward_) {
                       cursor_.t_win->FindNextCharacterAndCursorGoInCurrentLine(
                           c_to_find_);
                   } else {
                       cursor_.t_win->FindPrevCharacterAndCursorGoInCurrentLine(
                           c_to_find_);
                   }
               }});
    CHX_KEYMAP(",", {[this] {
                   if (find_forward_) {
                       cursor_.t_win->FindPrevCharacterAndCursorGoInCurrentLine(
                           c_to_find_);
                   } else {
                       cursor_.t_win->FindNextCharacterAndCursorGoInCurrentLine(
                           c_to_find_);
                   }
               }});

    // Buffer manangement
    CHX_KEYMAP("]b", {[this] { cursor_.t_win->NextBuffer(); }},
               {Mode::kNormal});
    CHX_KEYMAP("[b", {[this] { cursor_.t_win->PrevBuffer(); }},
               {Mode::kNormal});

    // esc
    CHX_KEYMAP("<esc>", {[this] {
                   if (IsPeel(mode_)) {
                       NotifyUser("");
                   }
                   ExitFromMode();
               }},
               {CHX_ALL_MODES}, {CHX_ALL_CONTEXTS});

    // command & search
    CHX_KEYMAP("/", {[this] {
                   search_foward_ = true;
                   GotoPeel(Mode::kPeelSearch);
               }},
               {Mode::kNormal}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("?", {[this] {
                   search_foward_ = false;
                   GotoPeel(Mode::kPeelSearch);
               }},
               {Mode::kNormal}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("N",
               {[this] { CursorGoSearch(!search_foward_, Count(), false); }},
               {Mode::kNormal}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("n",
               {[this] { CursorGoSearch(search_foward_, Count(), false); }},
               {Mode::kNormal}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP(":", {[this] { GotoPeel(); }}, {Mode::kNormal},
               {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<enter>", {[this] { GotoPeel(Mode::kPeelShow); }},
               {Mode::kNormal});
    CHX_KEYMAP("<c-r>", {[this] {
                   peel_->Paste();
                   layout_manager_->ArrangeLayout();
               }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<bs>", {[this] {
                   if (peel_->DeleteCharacterBeforeCursor() == kOk) {
                       editor_event_manager_.EmitEvent(
                           EditorEvent::kCommandCharEdit, nullptr);
                       layout_manager_->ArrangeLayout();
                   }
               }},
               {Mode::kPeelCommand}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<bs>", {[this] {
                   if (peel_->DeleteCharacterBeforeCursor() == kOk) {
                       editor_event_manager_.EmitEvent(
                           EditorEvent::kSearchCharEdit, nullptr);
                       layout_manager_->ArrangeLayout();
                   }
               }},
               {Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-w>", {[this] {
                   peel_->DeleteWordBeforeCursor();
                   layout_manager_->ArrangeLayout();
               }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<enter>", {[this] {
                   CommandHitEnter();
                   multirow_peel_keep_ = true;
               }},
               {Mode::kPeelCommand}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<enter>", {[this] {
                   SearchHitEnter();
                   multirow_peel_keep_ = true;
               }},
               {Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<left>", {[this] { peel_->CursorGoLeft(Count()); }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<right>", {[this] { peel_->CursorGoRight(Count()); }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-left>", {[this] { peel_->CursorGoPrevWordBegin(Count()); }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-right>", {[this] { peel_->CursorGoNextWordEnd(Count()); }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<home>", {[this] { peel_->CursorGoHome(); }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<end>", {[this] { peel_->CursorGoEnd(); }},
               {Mode::kPeelCommand, Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});

    // cmp & history
    CHX_KEYMAP("<c-space>", {[this] { TriggerCompletion(false); }},
               {Mode::kInsert, Mode::kPeelCommand}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-c>", {[this] { TriggerCompletion(false); }},
               {Mode::kInsert, Mode::kPeelCommand}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<tab>", {[this] {
                   if (CompletionTriggered()) {
                       if (completer_->Accept(cmp_menu_->Accept(), &cursor_) ==
                           kRetriggerCmp) {
                           completer_ = nullptr;
                           TriggerCompletion(true);
                           layout_manager_->ArrangeLayout();
                       } else {
                           completer_ = nullptr;
                       }
                   }
               }},
               {Mode::kPeelCommand}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<tab>", {[this] {
                   if (CompletionTriggered()) {
                       if (completer_->Accept(cmp_menu_->Accept(), &cursor_) ==
                           kRetriggerCmp) {
                           completer_ = nullptr;
                           TriggerCompletion(true);
                           layout_manager_->ArrangeLayout();
                       } else {
                           completer_ = nullptr;
                       }
                   } else {
                       cursor_.t_win->TabAtCursor();
                   }
               }},
               {Mode::kInsert});
    CHX_KEYMAP("<c-n>", {[this] {
                   if (CompletionTriggered()) {
                       cmp_menu_->SelectNext(1);
                       show_cmp_menu_ = true;
                   }
               }},
               {Mode::kInsert});
    CHX_KEYMAP("<c-n>", {[this] {
                   if (CompletionTriggered()) {
                       cmp_menu_->SelectNext(1);
                       show_cmp_menu_ = true;
                   } else {
                       peel_->NextHistoryItem(MangoPeel::HistoryType::kCmd);
                   }
               }},
               {Mode::kPeelCommand}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-n>", {[this] {
                   peel_->NextHistoryItem(MangoPeel::HistoryType::kSearch);
               }},
               {Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-p>", {[this] {
                   if (CompletionTriggered()) {
                       cmp_menu_->SelectPrev(1);
                       show_cmp_menu_ = true;
                   }
               }},
               {Mode::kInsert});
    CHX_KEYMAP("<c-p>", {[this] {
                   if (CompletionTriggered()) {
                       cmp_menu_->SelectPrev(1);
                       show_cmp_menu_ = true;
                   } else {
                       peel_->PrevHistoryItem(MangoPeel::HistoryType::kCmd);
                   }
               }},
               {Mode::kPeelCommand}, {CHX_ALL_CONTEXTS});
    CHX_KEYMAP("<c-p>", {[this] {
                   peel_->PrevHistoryItem(MangoPeel::HistoryType::kSearch);
               }},
               {Mode::kPeelSearch}, {CHX_ALL_CONTEXTS});

    // Edit
    CHX_KEYMAP("<bs>", {[this] {
                   if (cursor_.t_win->DeleteAtCursor() == kOk) {
                       editor_event_manager_.EmitEvent(
                           EditorEvent::kEditCharEdit, nullptr);
                   }
               }},
               {Mode::kInsert});
    CHX_KEYMAP("<c-w>", {[this] { cursor_.t_win->DeleteWordBeforeCursor(); }},
               {Mode::kInsert});
    CHX_KEYMAP("<enter>", {[this] { cursor_.t_win->AddStringAtCursor("\n"); }},
               {Mode::kInsert});
    CHX_KEYMAP("<c-r>", {[this] { cursor_.t_win->Paste(1); }}, {Mode::kInsert});
    CHX_KEYMAP("i", {[this] { GotoMode(Mode::kInsert); }}, {Mode::kNormal});
    CHX_KEYMAP("I", {[this] {
                   cursor_.t_win->CursorGoFirstNonBlank();
                   GotoMode(Mode::kInsert);
               }},
               {Mode::kNormal});  // TODO it
    CHX_KEYMAP("a", {[this] {
                   cursor_.t_win->CursorGoRight(Count());
                   GotoMode(Mode::kInsert);
               }},
               {Mode::kNormal});
    CHX_KEYMAP("A", {[this] {
                   cursor_.t_win->CursorGoEnd();
                   GotoMode(Mode::kInsert);
               }},
               {Mode::kNormal});
    CHX_KEYMAP("o", {[this] {
                   cursor_.t_win->NewLineUnderCursorline();
                   GotoMode(Mode::kInsert);
               }},
               {Mode::kNormal});
    CHX_KEYMAP("O", {[this] {
                   cursor_.t_win->NewLineAboveCursorline();
                   GotoMode(Mode::kInsert);
               }},
               {Mode::kNormal});
    CHX_KEYMAP("u", {[this] { cursor_.t_win->Undo(); }}, {Mode::kNormal});
    CHX_KEYMAP("<c-r>", {[this] { cursor_.t_win->Redo(); }}, {Mode::kNormal});
    CHX_KEYMAP("y", {[this] {
                   cursor_.t_win->Copy();
                   ExitFromMode();
               }},
               {CHX_SELECT_MODES});
    CHX_KEYMAP("y", {[this] {
                   mode_ = Mode::kOperatorPending;
                   pending_operator_ = Operator::kYank;
               }},
               {Mode::kNormal});
    CHX_KEYMAP("p", {[this] {
                   cursor_.t_win->Paste(Count());
                   ExitFromMode();
               }},
               {CHX_DEFAULT_MODES});
    CHX_KEYMAP("d", {[this] {
                   cursor_.t_win->Cut();
                   ExitFromMode();
               }},
               {CHX_SELECT_MODES});
    CHX_KEYMAP("d", {[this] {
                   mode_ = Mode::kOperatorPending;
                   pending_operator_ = Operator::kDelete;
               }},
               {Mode::kNormal});
    CHX_KEYMAP("\\>", {[this] {
                   cursor_.t_win->IndentSelection(Count());
                   ExitFromMode();
               }},
               {CHX_SELECT_MODES});
    CHX_KEYMAP("\\>", {[this] {
                   mode_ = Mode::kOperatorPending;
                   pending_operator_ = Operator::kIndent;
               }},
               {Mode::kNormal});
    CHX_KEYMAP("\\<", {[this] {
                   cursor_.t_win->UnindentSelection(Count());
                   ExitFromMode();
               }},
               {CHX_SELECT_MODES});
    CHX_KEYMAP("\\<", {[this] {
                   mode_ = Mode::kOperatorPending;
                   pending_operator_ = Operator::kUnindent;
               }},
               {Mode::kNormal});

    // A inner format, not exposed
    CHX_KEYMAP(
        "<space>f", {[this] {
            auto& path = cursor_.t_win->area_.buffer_->path();
            if (path.Empty()) {
                return;
            }
            try {
                const char* const argv[] = {"clang-format", "--assume-filename",
                                            path.AbsolutePath().c_str(),
                                            nullptr};
                Buffer* b = cursor_.t_win->area_.buffer_;
                int exit_code;
                std::string stdout;
                std::string stdin =
                    TextTree::TextView{b->Begin(), b->End()}.ToString();
                Result res = Exec(argv, &stdin, &stdout, nullptr, exit_code);
                if (res != kOk || exit_code != 0 || !CheckUtf8Valid(stdout)) {
                    return;
                }
                if (stdin == stdout) {
                    return;
                }
                cursor_.t_win->area_.b_view_->make_cursor_visible = true;
                Pos pos = FixCursorPos(cursor_.pos, stdout);
                // TODO: diff
                cursor_.t_win->area_.Replace(
                    {{0, 0},
                     {b->LineCnt() - 1,
                      b->GetLineView(b->LineCnt() - 1).Size()}},
                    stdout, &pos);
            } catch (OSException& e) {
                // TODO: notify
            }
        }},
        {Mode::kNormal});

    // Operator Pending motion / text object
    CHX_KEYMAP("d", {[this] {
                   // TODO
                   ExitFromMode();
               }},
               {Mode::kOperatorPending});
    CHX_KEYMAP("y", {[this] {
                   // TODO
                   ExitFromMode();
               }},
               {Mode::kOperatorPending});

    // Selection
    CHX_KEYMAP("s", {[this] {
                   cursor_.t_win->area_.StartLineSelection(cursor_.pos);
                   GotoMode(Mode::kSelectLine);
               }},
               {Mode::kNormal});
    CHX_KEYMAP("S", {[this] {
                   cursor_.t_win->area_.StartSelection(cursor_.pos);
                   GotoMode(Mode::kSelect);
               }},
               {Mode::kNormal});

    // Explorer
    CHX_KEYMAP("<space>e", {[this] { OpenExplorer(); }}, {Mode::kNormal});
    CHX_KEYMAP("q", {[this] { QuitExplorer(); }}, {Mode::kNormal},
               {Context::kExplorer});
    CHX_KEYMAP("<enter>", {[this] {
                   if (peel_->area_.height_ != 1) {
                       GotoPeel(Mode::kPeelShow);
                       return;
                   }
                   explorer_->EnterCurrentEntry();
               }},
               {Mode::kNormal}, {Context::kExplorer});
}

#define CHX_CMD command_manager_.AddCommand
void Editor::InitCommands() {
#define CHX_ENSURE_ARGEXITS(v) CHX_ASSERT(args[v].has_value())
    CHX_CMD({"quit", "q", "", {}, [this](const CommandArgs& args) {
                 (void)args;
                 Quit(false);
             }});
    CHX_CMD({"quit!", "q!", "", {}, [this](const CommandArgs& args) {
                 (void)args;
                 Quit(true);
             }});
    CHX_CMD({"help",
             "h",
             "",
             {Type::kString},
             [this](const CommandArgs& args) {
                 EnsureInEditorContext();
                 if (!args[0].has_value()) {
                     Help(kHelpDoc);
                 } else {
                     Help(std::get<std::string>(*args[0]));
                 }
             },
             1,
             1});
    CHX_CMD({"write",
             "w",
             "",
             {Type::kString},
             [this](const CommandArgs& args) {
                 if (context_ != Context::kEditor) return;
                 if (args[0].has_value()) {
                     SaveCurrentBufferAs(Path(std::get<std::string>(*args[0])));
                 } else {
                     SaveCurrentBuffer();
                 }
             },
             1,
             1});
    CHX_CMD({"edit",
             "e",
             "",
             {Type::kString},
             [this](const CommandArgs& args) {
                 CHX_ENSURE_ARGEXITS(0);
                 EnsureInEditorContext();
                 Edit(std::get<std::string>(*args[0]));
             },
             1});
    CHX_CMD({"buffer",
             "b",
             "",
             {Type::kString},
             [this](const CommandArgs& args) {
                 CHX_ENSURE_ARGEXITS(0);
                 EnsureInEditorContext();
                 const std::string& name_str = std::get<std::string>(*args[0]);
                 Buffer* b = buffer_manager_->FindBuffer(name_str);
                 if (b) {
                     cursor_.t_win->AttachBuffer(b);
                 }
             },
             1});
    CHX_CMD({"bdelete", "bd", "", {}, [this](const CommandArgs& args) {
                 (void)args;
                 if (context_ != Context::kEditor) return;
                 RemoveCurrentBuffer();
             }});
    CHX_CMD({"smile", "", "", {}, [this](const CommandArgs& args) {
                 (void)args;
                 NotifyUser(kSmile);
             }});
    CHX_CMD({"about", "", "", {Type::kString}, [this](const CommandArgs& args) {
                 (void)args;
                 NotifyUser(kVersionInfo);
             }});
#undef CHX_ENSURE_ARGEXITS
}

void Editor::PrintKey(const Terminal::KeyInfo& key_info) {
    bool ctrl = key_info.mod & Terminal::kCtrl;
    bool shift = key_info.mod & Terminal::kShift;
    bool alt = key_info.mod & Terminal::kAlt;
    bool motion = key_info.mod & Terminal::kMotion;
    (void)ctrl, (void)shift, (void)alt, (void)motion;
    char c[kMaxBytesUtf8Codepoint + 1];
    int len = UnicodeToUtf8(key_info.codepoint, c);
    c[len] = '\0';
    CHX_LOG_DEBUG(
        "ctrl {} shift {} alt {} motion {} special key {} codepoint "
        "\\U{:08X} char {}",
        ctrl, shift, alt, motion, static_cast<int>(key_info.special_key),
        key_info.codepoint, c);
}

void Editor::HandleBracketedPaste(std::string& bracketed_paste_buffer) {
    Terminal::KeyInfo key_info = term_.EventKeyInfo();

#ifndef NDEBUG
    if (global_opts_->GetOpt<bool>(kOptLogVerbose)) {
        PrintKey(key_info);
    }
#endif  // !NDEBUG

    if (key_info.IsSpecialKey() && key_info.mod == Terminal::kCtrl &&
        key_info.special_key == Terminal::SpecialKey::kEnter) {
        if (!IsPeel(mode_)) {
            bracketed_paste_buffer.push_back('\n');
        }
    } else if (key_info.IsSpecialKey()) {
        bracketed_paste_buffer.append(kReplacement);
        CHX_LOG_INFO("Unknown Special key in bracketed paste");
    } else {
        char c[kMaxBytesUtf8Codepoint + 1];
        int len = UnicodeToUtf8(key_info.codepoint, c);
        c[len] = '\0';
        CHX_ASSERT(len > 0);
        bracketed_paste_buffer.append(c);
    }
}

void Editor::HandleKey() {
    Terminal::KeyInfo key_info = term_.EventKeyInfo();

#ifndef NDEBUG
    if (global_opts_->GetOpt<bool>(kOptLogVerbose)) {
        PrintKey(key_info);
    }
#endif  // !NDEBUG

    // We treat one codepoint as a key event instead of one grapheme.
    // 1. It's a limitation on terminals now. We can't detect graphemes on
    // input event reliably, especially on ssh.
    // 2. Users can input single codepoints.
    // 3. Our keymaps only use special keys, or just ascii characters, which
    // users will be aware of.

    // If the editor is in count state, means user have already input a seq of
    // numbers, we calc the input here.
    if (input_state_ == InputState::kCount && !key_info.IsSpecialKey() &&
        key_info.codepoint >= '0' && key_info.codepoint <= '9') {
        count_ = count_ * 10 + key_info.codepoint - '0';
        return;
    }

    Result res = kKeyseqError;
    Keyseq* handler;
    if (key_info.IsSpecialKey() || key_info.codepoint <= CHAR_MAX) {
        res = keymap_manager_.FeedKey(key_info, handler);
    }
    if (res == kKeyseqDone) {
        handler->f();
        if (mode_ == Mode::kOperatorPending) {
            // key seq like 2d2l -> d4l
            op_pending_stored_count_ = count_;
        }
        if (input_state_ == InputState::kCount) {
            count_ = 0;
            input_state_ = InputState::kNone;
        }
    } else if (res == kKeyseqMatched) {
        // CHX_LOG_DEBUG("keymap matched");

        // Encounter a sequnce, we let multirow peel stay.
        if (!IsPeel(mode_) && peel_->area_.height_ > 1) {
            multirow_peel_keep_ = true;
        }
    } else if (res == kKeyseqError) {
        // Pure codepoints that are not handled by the keymap manager.
        // Use single codepoint to edit buffers is quite safe here.

        // We may use a codepoint as grapheme when we meet some ascii
        // characters, like '(', '[' '{', because they're very very rare as a
        // part of multi-codepoint graphemes.
        if (key_info.IsSpecialKey()) {
            count_ = 0;
            input_state_ = InputState::kNone;
            return;
        }

        if (mode_ == Mode::kInsert || mode_ == Mode::kPeelCommand ||
            mode_ == Mode::kPeelSearch) {
            char c[kMaxBytesUtf8Codepoint + 1];
            int len = UnicodeToUtf8(key_info.codepoint, c);
            c[len] = '\0';
            CHX_ASSERT(len > 0);
            Result res;
            if (IsPeel(mode_)) {
                res = peel_->AddStringAtCursor(c);
                layout_manager_->ArrangeLayout();
            } else {
                // We only support insert mode in kEditor context.
                CHX_ASSERT(context_ == Context::kEditor);
                res = cursor_.t_win->AddStringAtCursor(c);
            }
            if (res != kOk) {
                return;
            }
            EditorEvent ev;
            switch (mode_) {
                case Mode::kInsert:
                    ev = EditorEvent::kEditCharEdit;
                    break;
                case Mode::kPeelCommand:
                    ev = EditorEvent::kCommandCharEdit;
                    break;
                case Mode::kPeelSearch:
                    ev = EditorEvent::kSearchCharEdit;
                    break;
                default:
                    ev = EditorEvent::__kCount;
                    CHX_ASSERT("Can't reach here");
                    break;
            }
            editor_event_manager_.EmitEvent(ev, nullptr);
        } else if (key_info.codepoint >= '0' && key_info.codepoint <= '9' &&
                   input_state_ == InputState::kNone) {
            count_ = count_ * 10 + key_info.codepoint - '0';
            input_state_ = InputState::kCount;
        }
    } else {
        CHX_ASSERT(false);
    }
}

void Editor::HandleLeftClick(int s_row, int s_col) {
    Window* win = LocateWindow(s_col, s_row);
    if (!win) {
        return;
    }

    Window* prev_win = cursor_.focused;
    Pos prev_pos = cursor_.pos;  // TODO: check this logic.
    switch (context_) {
        case Context::kEditor:
            cursor_.t_win = static_cast<TextWindow*>(win);
        default:
            break;
    }
    win->SetCursorHint(s_row, s_col);

    if (mouse_.state == MouseState::kReleased) {
        if (win->IsSelectionActive()) {
            win->StopSelection();
            ExitFromMode();
        }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                now - mouse_.last_click_time)
                .count() <
            global_opts_->GetOpt<int64_t>(kOptCursorStartHoldingInterval)) {
            mouse_.state = MouseState::kLeftHolding;
            win->DoubleClick();
        } else {
            mouse_.last_click_time = now;
            mouse_.state = MouseState::kLeftNotReleased;
        }
    } else if (mouse_.state == MouseState::kLeftNotReleased) {
        mouse_.state = MouseState::kLeftHolding;
        if (win == prev_win && !(prev_pos == cursor_.pos)) {
            if (!win->StartSelection(prev_pos)) return;
            GotoMode(Mode::kSelect);
        }
    } else if (mouse_.state == MouseState::kLeftHolding) {
        if (win->IsSelectionActive()) {
            win->SelectionFollowCursor();
            return;
        }

        if (win == prev_win && !(prev_pos == cursor_.pos)) {
            if (!win->StartSelection(prev_pos)) return;
            GotoMode(Mode::kSelect);
        }
    }
}

void Editor::HandleRelease(int s_row, int s_col) {
    (void)s_row, (void)s_col;
    mouse_.state = MouseState::kReleased;
}

void Editor::HandleMouse() {
    // Only non peel
    if (IsPeel(mode_)) {
        return;
    }

    Terminal::MouseInfo mouse_info = term_.EventMouseInfo();
    switch (mouse_info.t) {
        using mk = Terminal::MouseKey;
        case mk::kLeft: {
            HandleLeftClick(mouse_info.row, mouse_info.col);
            break;
        }
        case mk::kRight: {
            mouse_.state = MouseState::kRightNotReleased;
            cursor_.focused->StopSelection();
            break;
        }
        case mk::kMiddle: {
            mouse_.state = MouseState::kMiddleNotReleased;
            cursor_.focused->StopSelection();
            break;
        }
        case mk::kRelease: {
            HandleRelease(mouse_info.row, mouse_info.col);
            break;
        }
        case mk::kWheelDown: {
            Window* win = LocateWindow(mouse_info.col, mouse_info.row);
            if (win) {
                win->ScrollRows(global_opts_->GetOpt<int64_t>(kOptScrollRows));
            }
            // Not locate any area, do nothing
            break;
        }
        case mk::kWheelUp: {
            Window* win = LocateWindow(mouse_info.col, mouse_info.row);
            if (win) {
                win->ScrollRows(-global_opts_->GetOpt<int64_t>(kOptScrollRows));
            }
            // Not locate any area, do nothing
            break;
        }
    }
}

void Editor::HandleResize() { layout_manager_->ArrangeLayout(); }

void Editor::Draw() {
    // TODO: do not redraw not modified part
    // TODO: should we redraw if any events trigger?

    // First clear the screen so we don't need to print spaces for blank
    // screen parts
    term_.Clear();

    switch (context_) {
        case Context::kEditor:
            text_window_->Draw(highlight_search_);
            break;
        case Context::kExplorer:
            explorer_->Draw(highlight_search_);
            break;
        default:
            CHX_ASSERT(false);
            CHX_LOG_ERROR("Can't reach here");
    }
    status_line_->Draw();
    peel_->Draw();

    // Put it at last so it can override some parts
    cmp_menu_->Draw();

    // Draw cursor
    if (cursor_.s_col == -1 && cursor_.s_row == -1) {
        // when not make visible
        term_.HideCursor();
    } else {
        term_.SetCursor(cursor_.s_col, cursor_.s_row);
    }

    term_.Present();
}

void Editor::PreProcess() {
    // Try Load All Buffers in all windows
    if (text_window_->area_.buffer_->state() == BufferState::kHaveNotRead) {
        try {
            text_window_->area_.buffer_->Load();
            // TODO: Not init if file is too big.
            // Or just schedule the init.
            TSTree* ts_tree =
                syntax_parser_->SyntaxInit(text_window_->area_.buffer_);
            text_window_->area_.buffer_->ts_tree() = ts_tree;
        } catch (Exception& e) {
            NotifyUser(fmt::format("buffer {} load error: {}",
                                   text_window_->area_.buffer_->Name(),
                                   e.what()));
        }
    }

    peel_->area_.MakeSureViewValid();
    switch (context_) {
        case Context::kEditor:
            text_window_->area_.MakeSureViewValid();
            if (!IsPeel(mode_)) {
                cursor_.t_win->MakeCursorVisible();
            } else {
                peel_->MakeCursorVisible();
            }
            break;
        case Context::kExplorer:
            if (!IsPeel(mode_)) {
                explorer_->MakeCursorVisible();
            } else {
                peel_->MakeCursorVisible();
            }
            break;
        default:
            CHX_ASSERT(false);
            CHX_LOG_ERROR("Can't reach here");
    }
}

Window* Editor::LocateWindow(int s_col, int s_row) {
    switch (context_) {
        case Context::kEditor:
            if (text_window_->area_.In(s_col, s_row)) {
                return text_window_.get();
            }
            break;
        case Context::kExplorer:
            if (explorer_->In(s_col, s_row)) {
                return explorer_.get();
            }
            break;
        default:
            CHX_ASSERT(false);
    }
    return nullptr;
}

Editor& Editor::GetInstance() {
    static Editor editor;
    return editor;
}

void Editor::Help(const std::string& doc_name) {
    auto p = Path(Path::GetAppRoot() + kDocsPath + doc_name);
    Buffer* b = buffer_manager_->FindBuffer(p);
    if (b == nullptr) {
        try {
            std::vector<std::string> all_docs =
                Path::ListUnderPath(Path::GetAppRoot() + kDocsPath);
            bool found = false;
            for (const auto& doc : all_docs) {
                if (doc == doc_name) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                NotifyUser(fmt::format("Can't found doc: {}", doc_name));
                return;
            }
            b = buffer_manager_->AddBuffer(Buffer(global_opts_.get(), p, true));
        } catch (FSException& e) {
            CHX_LOG_ERROR("AllDocs error: {}", e.what());
            return;
        }
    }
    cursor_.t_win->AttachBuffer(b);
}

void Editor::Quit(bool force) {
    bool have_not_saved = false;
    for (auto buffer = buffer_manager_->Begin();
         buffer != buffer_manager_->End(); buffer = buffer->next_) {
        if (buffer->state() == BufferState::kModified) {
            have_not_saved = true;
            break;
        }
    }
    if (!have_not_saved || force) {
        loop_->EndLoop();
    } else {
        NotifyUser("Some buffers have not saved, force quit with 'q!'");
    }
}

void Editor::GotoPeel(Mode mode) {
    CHX_ASSERT(!IsPeel(mode_));

    if (mode == Mode::kPeelShow && peel_->area_.height_ == 1) {
        return;
    }

    cursor_.focused->SaveView();
    GotoMode(mode);

    if (mode == Mode::kPeelCommand) {
        peel_->UserInputStart("");
    } else if (mode == Mode::kPeelSearch) {
        peel_->UserInputStart(search_foward_ ? "Forward: " : "Backward: ");
    } else if (mode == Mode::kPeelShow) {
        cursor_.pos = Pos{0, 0};
    }
}

void Editor::ExitFromMode() {
    switch (mode_) {
        case Mode::kNormal:
            highlight_search_ = false;
            break;
        case Mode::kInsert: {
            term_.SetCursorStyle(Terminal::CursorStyle::kBlock);
            break;
        }
        case Mode::kSelect:
            [[fallthrough]];
        case Mode::kSelectLine: {
            cursor_.focused->StopSelection();
            break;
        }
        case Mode::kOperatorPending: {
            break;
        }
        case Mode::kPeelCommand:
            [[fallthrough]];
        case Mode::kPeelSearch:
            term_.SetCursorStyle(Terminal::CursorStyle::kBlock);
            peel_->SetHistoryCursorToEnd();
            [[fallthrough]];
        case Mode::kPeelShow: {
            cursor_.focused->RestoreView();
            highlight_search_ = false;
            break;
        }
        default:
            CHX_ASSERT("Can't reach here");
    }
    mode_ = Mode::kNormal;
}

void Editor::GotoMode(Mode mode) {
    if (mode_ != Mode::kNormal) {
        ExitFromMode();
    }
    switch (mode) {
        case Mode::kInsert:
            [[fallthrough]];
        case Mode::kPeelCommand:
            [[fallthrough]];
        case Mode::kPeelSearch:
            term_.SetCursorStyle(Terminal::CursorStyle::kLine);
            break;
        default:
            break;
    }
    mode_ = mode;
}

void Editor::SearchCurrentWindow(const std::string& pattern) {
    // TODO: Maybe we can eliminate duplicate searching?
    if (!IsPeel(mode_)) {
        cursor_.focused->BuildSearchContext(pattern);
        CursorGoSearch(search_foward_, 1, true);
    } else {
        auto w = cursor_.focused;
        w->BuildSearchContext(pattern);
        highlight_search_ = w->ViewGoSearchResult(search_foward_, 1, true);
    }
}

void Editor::CursorGoSearch(bool next, size_t count, bool keep_current_if_one) {
    std::stringstream ss;
    auto w = cursor_.focused;
    auto& pattern = w->GetSearchPattern();
    SearchState state =
        w->CursorGoSearchResult(next, count, keep_current_if_one);
    ss << "Searching \"" << pattern << "\" ";
    if (state.total == 0) {
        ss << "[No result]";
    } else {
        ss << "[" << state.i << "/" << state.total << "]";
        highlight_search_ = true;
    }
    NotifyUser(ss.str());
}

void Editor::TriggerCompletion(bool autocmp) {
    if (CompletionTriggered()) {
        CancellCompletion();
    }

    bool in_peel = IsPeel(mode_);
    if (!in_peel) {
        completer_ = text_window_->area_.buffer_->completer();
    } else {
        completer_ = &peel_->completer_;
    }

    if (completer_ == nullptr) {
        if (autocmp) {
            return;
        }
        if (!in_peel) {
            NotifyUser("No completion source");
        }
        return;
    }

    std::vector<std::string> entries;
    completer_->Suggest(cursor_.pos, entries);
    if (entries.empty()) {
        completer_->Cancel();
        if (!autocmp && !in_peel) {
            NotifyUser("No completion");
        }
        completer_ = nullptr;
        return;
    }

    cmp_menu_->SetEntries(std::move(entries));
    cmp_menu_->visible() = true;
    show_cmp_menu_ = true;
}

void Editor::CancellCompletion() {
    if (!CompletionTriggered()) {
        return;
    }

    completer_->Cancel();
    completer_ = nullptr;
    cmp_menu_->Clear();
    cmp_menu_->visible() = false;
}

bool Editor::CompletionTriggered() { return completer_ != nullptr; }

void*& Editor::ContextManager::GetContext(ContextID id) {
    return contexts_[id];
}
void Editor::ContextManager::FreeContext(ContextID id) { contexts_.erase(id); }

void Editor::CommandHitEnter() {
    std::string_view input = peel_->GetUserInput();
    CommandArgs args;
    Command* c;
    Result res = command_manager_.EvalCommand(input, args, c);
    if (res == kCommandEmpty) {
        ExitFromMode();
        return;
    }

    peel_->AppendHistoryItem(mode_ == Mode::kPeelCommand
                                 ? MangoPeel::HistoryType::kCmd
                                 : MangoPeel::HistoryType::kSearch);

    if (res != kOk) {
        NotifyUser("Wrong Command");
        ExitFromMode();
        return;
    }

    // We first exit from peel, so we can use cursor_->in_window to get the
    // current window.
    ExitFromMode();
    c->f(args);
}

void Editor::SearchHitEnter() {
    ExitFromMode();
    auto input = peel_->GetUserInput();
    if (!input.empty()) {
        peel_->AppendHistoryItem(MangoPeel::HistoryType::kSearch);
    }
    SearchCurrentWindow(std::string(input));
}

void Editor::RemoveCurrentBuffer() {
    buffer_manager_->RemoveBuffer(cursor_.t_win->area_.buffer_);
}

void Editor::SaveCurrentBuffer() {
    try {
        Result res = cursor_.t_win->area_.buffer_->Write();
        if (res == kOk) {
            NotifyUser(
                fmt::format("\"{}\" saved",
                            cursor_.t_win->area_.buffer_->path().FileName()));
        } else if (res == kBufferNoBackupFile) {
            NotifyUser("Buffer no backup file");
        } else if (res == kBufferCannotLoad) {
            NotifyUser("Buffer can't load");
        } else if (res == kBufferReadOnly) {
            NotifyUser("Buffer read only");
        } else {
            assert("Can't reach here");
        }
    } catch (IOException& e) {
        std::string err_str = fmt::format("Buffer can't save: {}", e.what());
        NotifyUser(err_str);
    }
}

void Editor::SaveCurrentBufferAs(const Path& path) {
    Buffer* cur_b = cursor_.t_win->area_.buffer_;
    // We don't allow saving to the path of another buffer in order to avoid
    // some chaos.
    if (buffer_manager_->FindBuffer(path)) {
        NotifyUser("Same path buffer exists.");
        return;
    }
    try {
        Result res = cur_b->SaveAs(path);
        if (res == kBufferCannotLoad) {
            NotifyUser("Buffer can't load");
        } else if (res == kBufferReadOnly) {
            NotifyUser("Buffer read only");
        }
    } catch (IOException& e) {
        std::string err_str = fmt::format("Buffer can't save: {}", e.what());
        CHX_LOG_ERROR("{}", err_str);
        NotifyUser(err_str);
    }
}

void Editor::NotifyUser(std::string_view str) {
    peel_->ShowContent(str);
    layout_manager_->ArrangeLayout();
}

void Editor::StartAutoCompletionTimer() {
    // We start a timer, every terminal event will cancel it.
    // So if the timer is timeout, user input seems over, but we shouldn't rely
    // on that and think the current cursor pos is a real grapheme
    // end(user-percieved). Good time to trigger a autocmp.
    if (!autocmp_trigger_timer_) {
        autocmp_trigger_timer_ = std::make_unique<SingleTimer>(
            std::chrono::milliseconds(
                global_opts_->GetOpt<int64_t>(kOptInputIdleTimeout)),
            [this] { TriggerCompletion(true); });
    }
    loop_->timer_manager_.StartTimer(autocmp_trigger_timer_.get());
}

void Editor::StartSearchOnTypeTimer() {
    if (global_opts_->GetOpt<bool>(kOptHighlightOnSearch)) {
        if (!search_on_type_timer_) {
            search_on_type_timer_ = std::make_unique<SingleTimer>(
                std::chrono::milliseconds(
                    global_opts_->GetOpt<int64_t>(kOptInputIdleTimeout)),
                [this] { TrySearchOnType(); });
        }
        loop_->timer_manager_.StartTimer(search_on_type_timer_.get());
    }
}

void Editor::TrySearchOnType() {
    // Whether user is still searching?
    // If yes we search the pattern, otherwise we just ignore.
    if (mode_ != Mode::kPeelSearch) {
        return;
    }
    SearchCurrentWindow(std::string(peel_->GetUserInput()));
}

void Editor::StartupScreen() {
    if (buffer_manager_->Begin()->next_ != buffer_manager_->End() ||
        !buffer_manager_->Begin()->path().Empty()) {
        return;
    }

    size_t h = term_.Height();
    size_t w = term_.Width();

    if (h < kStartupHeight || w < kStartupWidth) {
        return;
    }

    size_t row0 = (h - kStartupHeight) / 2;
    size_t col0 = (w - kStartupWidth) / 2;

    term_.Clear();
    term_.HideCursor();
    auto scheme = global_opts_->GetOpt<ColorScheme>(kOptColorScheme);
    for (size_t r = row0; r < row0 + kStartupHeight; r++) {
        term_.Print(col0, r, scheme[kNormal], kStartup[r - row0]);
    }
    term_.Present();

    term_.Poll(-1);
    // If this is a resize event, we should handle it.
    if (term_.WhatEvent() == Terminal::EventType::kResize) {
        HandleResize();
    }
}

void Editor::Edit(const std::string& path) {
    Path p = Path(path);
    Buffer* b = buffer_manager_->FindBuffer(p);
    if (b) {
        cursor_.t_win->AttachBuffer(b);
        return;
    }
    b = buffer_manager_->AddBuffer(Buffer(global_opts_.get(), std::move(p)));
    try {
        buffer_monitor_->MonitorBuffer(b);
    } catch (OSException& e) {
        NotifyUser(fmt::format("Monitor buffer error: {}", e.what()));
    }
    cursor_.t_win->AttachBuffer(b);
}

void Editor::OpenExplorer() {
    CHX_ASSERT(!IsPeel(mode_));
    if (context_ != Context::kExplorer) {
        cursor_.focused->SaveView();
        context_ = Context::kExplorer;
        explorer_->RestoreView();
        cursor_.focused = explorer_.get();
        layout_manager_->ArrangeLayout();
    }
}

void Editor::QuitExplorer() {
    CHX_ASSERT(!IsPeel(mode_));
    CHX_ASSERT(context_ == Context::kExplorer);
    explorer_->SaveView();
    cursor_.t_win->RestoreView();
    context_ = Context::kEditor;
    cursor_.focused = cursor_.t_win;
    layout_manager_->ArrangeLayout();
}

void Editor::EnsureInEditorContext() {
    switch (context_) {
        case Context::kEditor:
            break;
        case Context::kExplorer: {
            QuitExplorer();
            break;
        }
        default:
            CHX_ASSERT(false);
            break;
    }
}

}  // namespace charxed
