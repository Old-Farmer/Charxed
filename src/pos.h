#pragma once

#include <cstddef>

namespace charxed {

// A Pos object represent a position in the line
struct Pos {
    size_t line;
    size_t byte_offset;

    bool operator==(Pos other) const noexcept {
        return byte_offset == other.byte_offset && line == other.line;
    }
    bool operator<(Pos other) const noexcept {
        return line < other.line ||
               (line == other.line && byte_offset < other.byte_offset);
    }
};

// Range represents a text range: [begin, end)
// NOTE:
// e.g. if the first line contains "abc", Use {{0, 0}, {0, 3}} to rep a line,
// Use {{0, 0}, {1, 0}} to rep the whole line with a '\n'.
struct Range {
    Pos begin;
    Pos end;

    bool PosBeforeMe(const Pos& pos) const { return pos < begin; }
    bool PosAfterMe(const Pos& pos) const { return !(pos < end); }

    // Pos is in Range ?
    bool PosInMe(const Pos& pos) const {
        return !(PosAfterMe(pos) || PosBeforeMe(pos));
    }

    bool RangeBeforeMe(const Range& range) {
        return range.end == begin || range.end < begin;
    }
    bool RangeAfterMe(const Range& range) {
        return end == range.begin || end < range.begin;
    }
    bool RangeOverlapMe(const Range& range) {
        return !RangeBeforeMe(range) && !RangeAfterMe(range);
    }
    bool RangeEqualMe(const Range& range) {
        return range.begin == begin && range.end == end;
    }
};

}  // namespace charxed
