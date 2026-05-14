#pragma once

#include <cstddef>
#include <list>
#include <optional>

#include "result.h"
#include "utils.h"
namespace mango {

// A class that can store linear history.
/*
This class have a sequence of history items and a cursor point to history point.

New history item always push at the cursor, and item pointed by the cursor and
items after the cursor will be deleted. After pushing an item, the cursor will
point to the end point of all history items and items at the very beginnings may
be deleted due to history cap limitaion.

cursor can move forward and backward for time traveling

e.g.
                                          cursor
                                             |
                                             V
| history item0(begin) | history item1 | history item2 | (end)


After pushing item3:
                                                         cursor
                                                            |
                                                            V
| history item0(begin) | history item1 | history item3 | (end)


After pushing item4, cap = 3:
                                                         cursor
                                                            |
                                                            V
| history item1(begin) | history item3 | history item4 | (end)

*/
template <typename T>
class History {
    struct HistoryItem {
        T data;
    };

   public:
    MGO_DEFAULT_CONSTRUCT_DESTRUCT(History);
    MGO_DELETE_COPY(History);
    MGO_DEFAULT_MOVE(History);

    // return kWrapHistory if history is wrapped,
    // otherwise return kOk.
    Result PushItem(const T& item, size_t cap) {
        // Delete all history iterms after cursor(include the item which cursor
        // points to)
        if (cursor_ != list_->end()) {
            MGO_ASSERT(!list_->empty());
            list_->erase(cursor_, list_->end());
            cursor_ = list_->end();
        }
        Result res = kOk;
        if (Size() >= cap) {
            res = kWrapHistory;
        }
        while (Size() >= cap) {
            list_->pop_front();
        }
        list_->push_back(item);
        return res;
    }
    // return kWrapHistory if history is wrapped,
    // otherwise return kOk.
    Result PushItem(T&& item, size_t cap) {
        // Delete all history iterms after cursor(include the item which cursor
        // points to)
        if (cursor_ != list_->end()) {
            MGO_ASSERT(!list_->empty());
            list_->erase(cursor_, list_->end());
            cursor_ = list_->end();
        }
        Result res = kOk;
        if (Size() >= cap) {
            res = kWrapHistory;
        }
        while (Size() >= cap) {
            list_->pop_front();
        }
        list_->push_back(std::move(item));
        return res;
    }
    // O(1) op
    size_t Size() { return list_->size(); }
    bool Empty() { return list_->empty(); }
    void Clear() {
        list_->clear();
        cursor_ = list_->end();
    }
    // Make sure Size > 0, otherwise behavior is undefined.
    // cursor can be after the latest item, at this situation return null
    std::optional<std::reference_wrapper<T>> GetItemAtCursor() {
        MGO_ASSERT(Size() != 0);
        if (cursor_ == list_->end()) {
            return {};
        }
        return *cursor_;
    }
    // Make sure Size > 0, otherwise behavior is undefined.
    // cursor can be the first item, at this situation return null
    std::optional<std::reference_wrapper<T>> GetItemJustBeforeCursor() {
        MGO_ASSERT(Size() != 0);
        if (cursor_ == list_->begin()) {
            return {};
        }
        auto prev_iter = cursor_;
        prev_iter--;
        return *prev_iter;
    }

    // return false if cursor can't move forward at this time.
    bool MoveCursorForward() {
        if (cursor_ == list_->end()) {
            return false;
        }
        cursor_++;
        return true;
    }

    // return false if cursor can't move backward at this time.
    bool MoveCursorBackward() {
        if (cursor_ == list_->begin()) {
            return false;
        }
        cursor_--;
        return true;
    }

    void MoveCursorEnd() { cursor_ = list_->end(); }

    bool IsCursorEnd() { return list_->end() == cursor_; }
    bool IsCursorBegin() { return list_->begin() == cursor_; }

    using ListIter = typename std::list<T>::iterator;

    // From tail to head iterator.
    class Iterator {
        ListIter cursor_;

       public:
        explicit Iterator(ListIter cursor) : cursor_(cursor) {}

        const T& operator*() const { return *cursor_; }

        Iterator operator++(int) {
            auto iter = *this;
            cursor_++;
            return iter;
        }

        bool operator!=(Iterator other) { return cursor_ != other.cursor_; }
        bool operator==(Iterator other) { return cursor_ == other.cursor_; }
    };

    Iterator Begin() { return Iterator(list_->rbegin()); }
    Iterator End() { return Iterator(list_->rend()); }

   private:
    // Use unique ptr to avoid a issue when std::list is moved, its iterator
    // will be become invalid
    std::unique_ptr<std::list<T>> list_ = std::make_unique<std::list<T>>();
    ListIter cursor_ = list_->end();
};

}  // namespace mango
