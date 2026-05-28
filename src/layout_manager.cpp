#include "layout_manager.h"

#include "explorer.h"
#include "mango_peel.h"
#include "status_line.h"
#include "text_window.h"

namespace charxed {

static constexpr size_t kStatusLineHeight = 1;

LayoutManager::LayoutManager(TextWindow* window, StatusLine* status_line,
                             MangoPeel* peel, Explorer* explorer,
                             Context* context)
    : window_(window),
      status_line_(status_line),
      peel_(peel),
      explorer_(explorer),
      context_(context) {}

void LayoutManager::ArrangeLayout() {
    size_t h = peel_->NeedHeight(term_->Width());
    ArrangeLayoutInner(h);
}

void LayoutManager::ArrangeLayoutInner(size_t peel_need_height) {
    size_t width = term_->Width();
    size_t height = term_->Height();

    size_t peel_real_height = std::min(
        peel_need_height, height % 2 == 0 ? height / 2 - 1 : height / 2);

    size_t window_height = height - kStatusLineHeight - peel_real_height;
    switch (*context_) {
        case Context::kEditor:
            window_->area_.col_ = 0;
            window_->area_.row_ = 0;
            window_->area_.width_ = width;
            window_->area_.height_ = window_height;
            break;
        case Context::kExplorer:
            explorer_->area_.col_ = 0;
            explorer_->area_.row_ = 0;
            explorer_->area_.width_ = width;
            explorer_->area_.height_ = window_height;
            break;
        default:
            CHX_ASSERT(false);
            break;
    }

    status_line_->row_ = window_height;
    status_line_->width_ = width;

    peel_->area_.row_ = height - peel_real_height;
    peel_->area_.width_ = width;
    peel_->area_.height_ = peel_real_height;

    peel_height_ = peel_need_height;
    peel_buffer_version_ = peel_->buffer_.version();
}

}  // namespace charxed
