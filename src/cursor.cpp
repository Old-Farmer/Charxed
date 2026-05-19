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

}  // namespace charxed
