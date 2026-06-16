#pragma once

#include "result.h"
#include "utf8proc.h"
#include "utils.h"

namespace charxed {

using Codepoint = utf8proc_int32_t;
constexpr size_t kMaxBytesUtf8Codepoint = 4;

inline bool IsUtf8BeginByte(char b) {
    // != 10uvwxyz
    return (static_cast<std::byte>(b) >> 6) != static_cast<std::byte>(0b10);
}

inline bool IsUTf8ByteAscii(char b) {
    // == 0tuvwxyz
    return b & (static_cast<char>(1) << 7);
}

// decode str to a codepoint
// if success, kOk return and len will be set to the byte consumed, out will be
// the codepoint.
// otherwise, kInvalidCoding will return.
inline Result Utf8ToUnicode(const char* in, size_t len, int& byte_eat,
                            Codepoint& out) {
    CHX_ASSERT(len != 0);
    if ((byte_eat = utf8proc_iterate(
             reinterpret_cast<const utf8proc_uint8_t*>(in), len, &out)) < 0) {
        return kInvalidCoding;
    }
    return kOk;
}

// Encode a codepoint, out buf must longer than 4 bytes
// On success, return encoded str len
// otherwise, return 0;
inline int UnicodeToUtf8(uint32_t in, char* out) {
    return utf8proc_encode_char(in, reinterpret_cast<utf8proc_uint8_t*>(out));
}

bool CheckUtf8Valid(std::string_view str);

}  // namespace charxed
