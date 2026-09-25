#include <Python.h>

#include <catch2/catch_test_macros.hpp>
#include <gsl/gsl>

#include "file.h"
#include "fs.h"
#include "options.h"
#include "script_runtime.h"

using namespace charxed;

TEST_CASE("python test") {
    Path::GetCwdSys();
    Path::GetAppRootSys();
    Path::GetHomeSys();

    GlobalOpts opts;
    auto& r = ScriptRuntime::GetInstance();
    r.Init(&opts);
    r.RunStr(R"(
with open("python_text.txt", "w", encoding="utf-8") as f:
    f.write("Hello from python")
)");

    File f = File("python_text.txt", "r");
    EOLSeq eol;
    std::string str = f.ReadAll(eol);
    REQUIRE(str == "Hello from python");
}
