#pragma once

#include <string_view>
#include <vector>

#include "character.h"

namespace charxed {

// InBytes work on byte.
// InCharacter work on grapheme.
// Any others work on codepoint

// work on byte.
std::vector<std::string_view> StrSplit(std::string_view str,
                                       char delimiter = kSpaceChar);

// NOTE: Bytes cmpare need two strings in same encoding and have the same
// normalization, or the result will not be that accurate.

// filter_same_size means if sub size == str size, functions will return false.

bool StrPrefixInBytes(const std::string_view sub, const std::string_view str,
                      bool filter_same_size);

bool StrFuzzyMatchInBytes(const std::string_view sub,
                          const std::string_view str, bool filter_same_size);

bool StrFuzzyMatchInBytes(const TextTree::TextView& sub,
                          const TextTree::TextView& str, bool filter_same_size);

// work on grapheme
TextTree::TextView FindPath(const TextTree::TextView& line,
                            TextTree::Iterator iter);

// work on codepoint
// from begin of the line, count x indentations and stop, return the iter.
TextTree::Iterator IndentationEnd(size_t count, const TextTree::TextView& line,
                                  int tabstop);

}  // namespace charxed
