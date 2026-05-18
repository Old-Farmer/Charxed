#include "str.h"

#include <unordered_set>

namespace charxed {

namespace {
const std::unordered_set<char> kPathStopCharacter = {
    '[', ']', '(', ')', '{', '}', '\"', '`', ',',
};
}

std::vector<std::string_view> StrSplit(std::string_view str, char delimiter) {
    std::vector<std::string_view> ret;

    size_t begin = 0;
    bool found_non_delimiter = false;
    for (size_t i = 0; i < str.size(); i++) {
        if (str[i] == delimiter) {
            if (found_non_delimiter) {
                ret.emplace_back(str.data() + begin, i - begin);
            }
            found_non_delimiter = false;
        } else {
            if (!found_non_delimiter) {
                found_non_delimiter = true;
                begin = i;
            }
        }
    }
    if (found_non_delimiter) {
        ret.emplace_back(str.data() + begin, str.size() - begin);
    }
    return ret;
}

bool StrPrefixInBytes(const std::string_view sub, const std::string_view str,
                      bool filter_same_size) {
    if (filter_same_size ? str.size() <= sub.size() : str.size() < sub.size()) {
        return false;
    }
    for (size_t i = 0; i < sub.size(); i++) {
        if (sub[i] != str[i]) {
            return false;
        }
    }
    return true;
}

bool StrFuzzyMatchInBytes(const std::string_view sub,
                          const std::string_view str, bool filter_same_size) {
    if (sub.size() == 0) {
        return false;
    }

    if (filter_same_size ? str.size() <= sub.size() : str.size() < sub.size()) {
        return false;
    }

    if (sub[0] != str[0]) {
        return false;
    }

    size_t i_sub = 1, i_str = 1;
    while (i_sub < sub.size() && i_str < str.size()) {
        if (sub[i_sub] == str[i_str]) {
            i_sub++;
            i_str++;
        } else {
            i_str++;
        }
    }
    return i_sub == sub.size();
}

bool StrFuzzyMatchInBytes(const TextTree::TextView& sub,
                          const TextTree::TextView& str,
                          bool filter_same_size) {
    if (sub.Size() == 0) {
        return false;
    }

    if (filter_same_size ? str.Size() <= sub.Size() : str.Size() < sub.Size()) {
        return false;
    }

    auto sub_iter = sub.begin;
    auto str_iter = str.begin;
    if (sub_iter.ThisByte() != str_iter.ThisByte()) {
        return false;
    }

    sub_iter.NextByte();
    str_iter.NextByte();
    while (sub_iter != sub.end && str_iter != str.end) {
        if (sub_iter.ThisByte() == str_iter.ThisByte()) {
            sub_iter.NextByte();
            str_iter.NextByte();
        } else {
            str_iter.NextByte();
        }
    }
    return sub_iter == sub.end;
}

TextTree::TextView FindPath(const TextTree::TextView& line,
                            TextTree::Iterator iter) {
    // expand to left and right
    auto right = iter;
    Character c;
    while (right != line.end) {
        auto next = NextCharacter(right, line.end, c);
        char asc;
        if (c.Ascii(asc) && kPathStopCharacter.count(asc)) {
            break;
        }
        right = next;
    }
    auto left = iter;
    while (left != line.begin) {
        auto prev = PrevCharacter(left, line.begin, c);
        char asc;
        if (c.Ascii(asc) && kPathStopCharacter.count(asc)) {
            break;
        }
        left = prev;
    }
    return {left, right};
}

}  // namespace charxed
