#include "clipboard.h"

#include <gsl/util>

#include "catch2/catch_test_macros.hpp"
#include "logging.h"

using namespace charxed;

TEST_CASE("XClipboard test") {
    LogInit("clipboard_test.log");
    auto _ = gsl::finally([] { LogDeinit(); });

    REQUIRE(getenv("HOME") != nullptr);

    auto clipboard = XClipBoard();

    std::string content("hello");
    clipboard.SetContent(content, false);
    bool lines;
    auto got_content = clipboard.GetContent(lines);
    REQUIRE(got_content == content);
    REQUIRE(!lines);

    // Do it again
    got_content = clipboard.GetContent(lines);
    REQUIRE(got_content == content);
    REQUIRE(!lines);

    // line semantic
    content = "world!";
    clipboard.SetContent(content, true);
    got_content = clipboard.GetContent(lines);
    REQUIRE(got_content == content);
    REQUIRE(lines);
}
