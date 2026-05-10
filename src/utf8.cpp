#include "utf8.h"

namespace mango {

bool CheckUtf8Valid(std::string_view str) {
    size_t offset = 0;
    size_t end_offset = str.size();
    Codepoint codepoint;
    while (offset < end_offset) {
        int byte_len;
        if (kInvalidCoding == Utf8ToUnicode(&str[offset], end_offset - offset,
                                            byte_len, codepoint)) {
            return false;
        }
        offset += byte_len;
    }
    return true;
}

}  // namespace mango
