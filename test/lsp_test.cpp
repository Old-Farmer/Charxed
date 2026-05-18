#include "catch2/catch_test_macros.hpp"
#include "gsl/gsl"
#include "logging.h"

using namespace charxed;

TEST_CASE("lsp") {
    LogInit("keyseq_manager_test.log");
    auto _ = gsl::finally([] { LogDeinit(); });
}
