#include "fs_monitor.h"

#include <sys/inotify.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "buffer.h"
#include "buffer_manager.h"
#include "cursor.h"
#include "exception.h"
#include "fs.h"
#include "syntax.h"
#include "window.h"

namespace charxed {

namespace {
constexpr uint32_t kWatchedFsEvent = IN_MOVE | IN_MODIFY | IN_DELETE_SELF;

struct InotifyEvent {
    int wd;
    uint32_t mask;
    uint32_t cookie;
    std::string name;
    bool rename_pair;
};
};  // namespace

FSMonitor::FSMonitor() {
    fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd_ == -1) {
        throw OSException(errno, "inotify_init1 error: {}", strerror(errno));
    }
}

FSMonitor::~FSMonitor() { close(fd_); }

// throw OSException
void FSMonitor::MonitorDir(std::string_view dir) {
    auto iter = dir_to_wd_.find(dir);
    if (iter != dir_to_wd_.end()) {
        wd_to_dir_[iter->second].ref_cnt++;
        return;
    }

    std::string copied_dir(dir.data(), dir.size());
    int wd = inotify_add_watch(fd_, copied_dir.c_str(), kWatchedFsEvent);
    if (wd == -1) {
        throw OSException(errno, "inotify_add_watch error: {}",
                          strerror(errno));
    }
    wd_to_dir_[wd] = {std::move(copied_dir), 1};
    dir_to_wd_[wd_to_dir_[wd].dir] = wd;
}

void FSMonitor::UnmonitorDir(std::string_view dir) {
    auto iter = dir_to_wd_.find(dir);
    if (iter == dir_to_wd_.end()) {
        return;
    }
    if (--wd_to_dir_[iter->second].ref_cnt == 0) {
        int wd = iter->second;
        dir_to_wd_.erase(iter);
        wd_to_dir_.erase(wd);
        inotify_rm_watch(fd_, wd);
    }
}

bool FSMonitor::HandleFsEvents() {
    alignas(alignof(inotify_event)) char buf[4096];
    std::vector<InotifyEvent> events;

    while (true) {
        ssize_t sz = read(fd_, buf, sizeof(buf));
        if (sz == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            throw OSException(errno, "read error: {}", strerror(errno));
        }

        inotify_event* event;
        for (char* ptr = buf; ptr < buf + sz;
             ptr += sizeof(inotify_event) + event->len) {
            event = reinterpret_cast<inotify_event*>(ptr);

            events.push_back({event->wd, event->mask, event->cookie,
                              std::string(event->name), false});
        }
    }

    bool res = false;

    // Check rename pairs
    for (auto& event : events) {
        if (event.mask & IN_MOVED_FROM) {
            for (auto& event2 : events) {
                if (event2.mask & IN_MOVED_TO &&
                    event2.cookie == event.cookie) {
                    event.rename_pair = true;
                    event2.rename_pair = true;
                    res = res || OnFsRenameEvent(wd_to_dir_[event.wd].dir,
                                                 event.mask & IN_ISDIR,
                                                 event.name, event2.name);
                    break;
                }
            }
        }
    }

    for (const auto& event : events) {
        if (event.mask & IN_DELETE_SELF) {
            const std::string& dir = wd_to_dir_[event.wd].dir;
            dir_to_wd_.erase(dir);
            wd_to_dir_.erase(event.wd);
            res = true;
        } else if (event.mask & IN_MODIFY) {
            res = res || OnFsEvent(FsEvent::kModify, wd_to_dir_[event.wd].dir,
                                   event.mask & IN_ISDIR, event.name);
        } else if (event.mask & IN_MOVED_TO && !event.rename_pair) {
            res = res || OnFsEvent(FsEvent::kMovedTo, wd_to_dir_[event.wd].dir,
                                   event.mask & IN_ISDIR, event.name);
        }
    }
    return res;
}

BufferFSMonitor::BufferFSMonitor(BufferManager* buffer_manager, Cursor* cursor,
                                 SyntaxParser* syntax_parser)
    : buffer_manager_(buffer_manager),
      cursor_(cursor),
      syntax_parser_(syntax_parser) {}

void BufferFSMonitor::MonitorBuffer(Buffer* b) {
    CHX_ASSERT(!b->path().Empty());
    auto dir = b->path().Dir();
    MonitorDir(dir);
}

void BufferFSMonitor::UnmonitorBuffer(Buffer* b) {
    CHX_ASSERT(!b->path().Empty());
    auto dir = b->path().Dir();
    UnmonitorDir(dir);
}

bool BufferFSMonitor::OnFsEvent(FsEvent event, std::string_view dir,
                                bool is_dir, std::string_view name) {
    // Currently we don't consider dir.
    if (is_dir) {
        return false;
    }

    switch (event) {
        case FsEvent::kMovedTo:
        case FsEvent::kModify: {
            auto path = std::string(dir);
            path.append(name);
            return BufferModified(path);
        }
    }
    return true;  // Make compiler happy
}

bool BufferFSMonitor::OnFsRenameEvent(std::string_view dir, bool is_dir,
                                      std::string_view from_name,
                                      std::string_view to_name) {
    // Currently we don't consider dir.
    if (is_dir) {
        return false;
    }

    if (from_name.size() >= kSwapSuffix.size() &&
        kSwapSuffix == from_name.substr(from_name.size() - kSwapSuffix.size(),
                                        kSwapSuffix.size())) {
        return false;
    }

    auto path = std::string(dir);
    path.append(to_name);
    return BufferModified(path);
}

bool BufferFSMonitor::BufferModified(std::string_view path) {
    Buffer* b = buffer_manager_->FindBuffer(path);
    if (!b || !b->IsLoad() || b->state() == BufferState::kModified) {
        return false;
    }

    bool is_showed =
        cursor_->in_window && cursor_->in_window->area_.buffer_ == b;

    try {
        if (is_showed) {
            Pos cursor_pos_hint;
            b->Reload(&cursor_->pos, cursor_pos_hint);
            cursor_->pos = cursor_pos_hint;
        } else {
            Pos cursor_pos_hint;
            b->Reload(nullptr, cursor_pos_hint);
        }
        syntax_parser_->ParseSyntaxAfterEdit(b);
    } catch (Exception& e) {
        if (is_showed) {
            cursor_->pos = {0, 0};
            cursor_->DontHoldColWant();
        }
        return true;
    }
    return true;
}

}  // namespace charxed
