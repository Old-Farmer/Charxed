#include "cursor.h"

#include <vector>

namespace charxed {

Pos FixCursorPos(Pos pos, std::string_view str) {
    std::vector<std::string_view> lines;
    size_t sz = str.size();
    size_t line_start = 0;
    size_t i = 0;
    for (; i < sz; i++) {
        if (str[i] == '\n') {
            lines.push_back({str.data() + line_start, i - line_start});
            line_start = i + 1;
        }
    }
    if (sz == 0) {
        lines.push_back(std::string_view{});
    } else {
        lines.push_back({str.data() + line_start, i - line_start});
    }

    size_t line_cnt = lines.size();
    if (pos.line >= line_cnt) {
        pos = {line_cnt - 1, lines[line_cnt - 1].size()};
    } else {
        pos.byte_offset = std::min(lines[pos.line].size(), pos.byte_offset);
    }
    return pos;
}

Pos FixCursorPosAfterAdd(Pos pos, Pos add_pos, Pos end_pos,
                         bool prefer_begin_pos) {
    if (pos < add_pos) {
        return pos;
    } else if (pos == add_pos) {
        return prefer_begin_pos ? add_pos : end_pos;
    } else if (pos.line == add_pos.line) {
        Pos new_pos = end_pos;
        new_pos.byte_offset += pos.byte_offset - add_pos.byte_offset;
        return new_pos;
    } else {
        Pos new_pos = pos;
        new_pos.line += end_pos.line - add_pos.line;
        return new_pos;
    }
}
Pos FixCursorPosAfterDelete(Pos pos, const Range& range) {
    if (range.PosBeforeMe(pos)) {
        return pos;
    } else if (range.PosInMe(pos)) {
        return range.begin;
    } else if (range.begin.line == pos.line) {
        Pos new_pos = range.begin;
        new_pos.byte_offset += pos.byte_offset - range.end.byte_offset;
        return new_pos;
    } else {
        Pos new_pos = pos;
        new_pos.line -= range.end.line - range.end.line;
        return new_pos;
    }
}
Pos FixCursorPosAfterReplace(Pos pos, const Range& range, Pos end_pos,
                             bool prefer_begin_pos) {
    pos = FixCursorPosAfterDelete(pos, range);
    return FixCursorPosAfterAdd(pos, range.begin, end_pos, prefer_begin_pos);
}

}  // namespace charxed
