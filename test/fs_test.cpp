#include "fs.h"

#include "catch2/catch_test_macros.hpp"

using namespace charxed;

TEST_CASE("Path Normalize") {
    const std::string p = "/a/b/c/../.././/x/";
    REQUIRE(Path::Normalize(p) == "/a/x/");
}
