#pragma once

#include <string>

#include "event_loop.h"
#include "state.h"
#include "utils.h"

namespace charxed {

class BufferManager;
struct Cursor;
class SyntaxParser;
class Buffer;

// FSMonitor is a class that can monitor some directories and run callbacks when
// some FS event happens.
class FSMonitor {
   public:
    // throw OSException
    FSMonitor();
    virtual ~FSMonitor();
    CHX_DELETE_COPY(FSMonitor);
    CHX_DELETE_MOVE(FSMonitor);

    // Events We care about.
    // Currently we only care about rename and modify.
    enum class FsEvent {
        kMovedTo,
        kModify,
    };

    // throw OSException.
    // The dir will be monitored and ref cnt ++.
    void MonitorDir(std::string_view dir);

    // The dir ref cnt --.
    // if ref cnt == 0, dir will not be monitored.
    void UnmonitorDir(std::string_view dir);

    // throw OSException
    // return true if fs events is not ignored by us.
    bool HandleFsEvents();

    EventFD fd() { return fd_; }

   private:
    // Please implement the following 2 callbacks.

    // return true if fs events is not ignored by us.
    virtual bool OnFsEvent(FsEvent event, std::string_view dir, bool is_dir,
                           std::string_view name) = 0;
    // return true if fs events is not ignored by us.
    virtual bool OnFsRenameEvent(std::string_view dir, bool is_dir,
                                 std::string_view from_name,
                                 std::string_view to_name) = 0;

    EventFD fd_;

    struct DirContext {
        std::string dir;
        size_t ref_cnt = 0;
    };

    std::unordered_map<int, DirContext> wd_to_dir_;
    std::unordered_map<std::string_view, int> dir_to_wd_;
};

class BufferFSMonitor : public FSMonitor {
   public:
    // throw OSException
    BufferFSMonitor(BufferManager* buffer_manager, Cursor* cursor,
                    SyntaxParser* syntax_parser, Mode* mode, Context* context);
    ~BufferFSMonitor() override {}
    CHX_DELETE_COPY(BufferFSMonitor);
    CHX_DELETE_MOVE(BufferFSMonitor);

    // throw OSException
    // b should be a file backup buffer.
    void MonitorBuffer(Buffer* b);
    void UnmonitorBuffer(Buffer* b);

   private:
    bool OnFsEvent(FsEvent event, std::string_view dir, bool is_dir,
                   std::string_view name) override;
    bool OnFsRenameEvent(std::string_view dir, bool is_dir,
                         std::string_view from_name,
                         std::string_view to_name) override;
    bool BufferModified(std::string_view path);

    BufferManager* buffer_manager_;
    Cursor* cursor_;
    SyntaxParser* syntax_parser_;

    Mode* mode_;
    Context* context_;
};

}  // namespace charxed
