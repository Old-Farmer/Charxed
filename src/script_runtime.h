#pragma once

#include <Python.h>

#include <string>

#include "utils.h"

namespace charxed {

class GlobalOpts;

class ScriptRuntime {
   public:
    ~ScriptRuntime();
    CHX_DELETE_COPY(ScriptRuntime);
    CHX_DELETE_MOVE(ScriptRuntime);

    static ScriptRuntime& GetInstance();

    // throw ScriptRuntimeException
    void Init(GlobalOpts* global_opts);

    // throw File ctor exceptions
    void RunFile(const std::string& path);

    void RunStr(const std::string& str);

   private:
    ScriptRuntime() {}

    // path should be absolute path
    static void AddModuleSearchPath(const std::string_view path);
    // Register editor's api
    static void RegisterApi();

    PyThreadState* tstate_;

    GlobalOpts* global_opts_;
};

}  // namespace charxed
