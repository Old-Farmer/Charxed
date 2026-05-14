#include "history.h"

#include "catch2/catch_test_macros.hpp"

using namespace mango;

TEST_CASE("history_test") {
    History<int> history;

    std::vector<int> items = {1, 2, 3};
    for (auto item : items) {
        history.PushItem(item, items.size());
    }

    REQUIRE(history.Size() == items.size());
    REQUIRE(history.GetItemJustBeforeCursor().has_value());
    REQUIRE(*history.GetItemJustBeforeCursor() == 3);
    REQUIRE(!history.GetItemAtCursor().has_value());
    REQUIRE(history.MoveCursorBackward());
    REQUIRE(history.MoveCursorBackward());
    REQUIRE(history.MoveCursorBackward());
    REQUIRE(!history.MoveCursorBackward());
    REQUIRE(history.MoveCursorForward());
    REQUIRE(history.MoveCursorForward());
    REQUIRE(history.MoveCursorForward());
    REQUIRE(!history.MoveCursorForward());

    size_t i = items.size() - 1;
    for (auto iter = history.Begin(); iter != history.End(); iter++, i--) {
        REQUIRE(*iter == items[i]);
    }
}
