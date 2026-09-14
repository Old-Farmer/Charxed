#pragma once

#include "term.h"
#include "utils.h"

namespace charxed {

struct Cursor;
class GlobalOpts;
enum class Mode;
enum class Context;

// the status line stay above the mango peel.
// Only one row.
class StatusLine {
   public:
    StatusLine(Cursor* cursor, GlobalOpts* options, Mode* mode,
               Context* context, const KeyseqManager* keyseq_manager,
               const std::vector<Terminal::KeyInfo>* pending_keys);
    ~StatusLine() = default;
    CHX_DELETE_COPY(StatusLine);
    CHX_DEFAULT_MOVE(StatusLine);

    void Draw();

   public:
    size_t width_ = 0;
    size_t row_ = 0;  // top left corner x related to the whole screen

   private:
    std::string left_str_;
    std::string right_str_;
    std::string pending_keys_buf_;
    Cursor* cursor_;
    GlobalOpts* global_opts_;
    Mode* mode_;
    Context* context_;
    const KeyseqManager* keyseq_manager_;
    const std::vector<Terminal::KeyInfo>* pending_keys_;

    Terminal* term_ = &Terminal::GetInstance();
};

}  // namespace charxed
