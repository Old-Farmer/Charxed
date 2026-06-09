#pragma once

#include "pos.h"

namespace charxed {

class Buffer;

struct Selection {
    Pos anchor;
    Pos head;

    Selection() {}
    Selection(Pos _anchor, Pos _head) : anchor(_anchor), head(_head) {}

    virtual Range ToSelectRange(const Buffer* buffer) const = 0;
    virtual Range ToDeleteRange(const Buffer* buffer) const {
        return ToSelectRange(buffer);
    }
    virtual bool LineSemantic() const { return false; }
    virtual ~Selection() {}  // Just quiet compiler warning
};

// normal selection
struct NormalSelection : Selection {
    bool inclusive_end;  // Normally, selection is exclusive end semantic, but
                         // sometimes we want inclusive end.

    NormalSelection() {}
    NormalSelection(Pos _anchor, Pos _head, bool _inclusive_end = false)
        : Selection(_anchor, _head), inclusive_end(_inclusive_end) {}

    Range ToSelectRange(const Buffer* buffer) const override;
};

// Line selection
struct LineSelection : Selection {
    LineSelection() {}
    LineSelection(Pos _anchor, Pos _head) : Selection(_anchor, _head) {}

    Range ToSelectRange(const Buffer* buffer) const override;
    Range ToDeleteRange(const Buffer* buffer) const override;
    bool LineSemantic() const override { return true; }
};

}  // namespace charxed
