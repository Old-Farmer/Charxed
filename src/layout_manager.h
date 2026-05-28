#pragma once

#include "term.h"

namespace charxed {

class TextWindow;
class StatusLine;
class MangoPeel;
class Explorer;
enum class Context;

class LayoutManager {
   public:
    LayoutManager(TextWindow* window, StatusLine* status_line, MangoPeel* peel,
                  Explorer* explorer, Context* context);

    void ArrangeLayout();

   private:
    void ArrangeLayoutInner(size_t peel_need_height);

   private:
    TextWindow* window_;
    StatusLine* status_line_;
    MangoPeel* peel_;
    Explorer* explorer_;
    Context* context_;
    Terminal* term_ = &Terminal::GetInstance();

    size_t peel_height_;
    int64_t peel_buffer_version_;
};

}  // namespace charxed
