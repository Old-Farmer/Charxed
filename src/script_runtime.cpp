#include "script_runtime.h"

#include <gsl/util>

#include "exception.h"
#include "file.h"
#include "fs.h"

namespace charxed {

ScriptRuntime::~ScriptRuntime() { Py_Finalize(); }

ScriptRuntime& ScriptRuntime::GetInstance() {
    static ScriptRuntime runtime;
    return runtime;
}

void ScriptRuntime::Init(GlobalOpts* global_opts) {
    global_opts_ = global_opts;

    RegisterApi();

    const std::string lib_path = Path::GetAppRoot() + "lib/";
    PyConfig config;
    auto _ = gsl::finally([&config] { PyConfig_Clear(&config); });
    PyConfig_InitIsolatedConfig(&config);
    std::string python_home = Path::GetAppRoot() + "python";
    PyStatus status;
    status =
        PyConfig_SetBytesString(&config, &config.home, python_home.c_str());
    if (PyStatus_Exception(status)) {
        throw ScriptRuntimeException("Python runtime init error: {}",
                                     status.err_msg);
    }
    status = Py_InitializeFromConfig(&config);
    if (PyStatus_Exception(status)) {
        throw ScriptRuntimeException("Python runtime init error: {}",
                                     status.err_msg);
    }
    AddModuleSearchPath(Path::GetAppRoot() + "resource/python");
    PyObject* module = PyImport_ImportModule("chx");
    auto _1 = gsl::finally([module] { Py_XDECREF(module); });
    if (module == nullptr) {
        PyErr_Print();
        throw ScriptRuntimeException("Python import chx module error");
    }
    tstate_ = PyEval_SaveThread();
}

void ScriptRuntime::AddModuleSearchPath(const std::string_view path) {
    PyObject* sys_path = PySys_GetObject("path");
    CHX_ASSERT(sys_path != nullptr);
    PyObject* path_str = PyUnicode_FromStringAndSize(path.data(), path.size());
    CHX_ASSERT(path_str != nullptr);
    int res = PyList_Append(sys_path, path_str);
    if (res != 0) {
        PyErr_Print();
    }
    CHX_ASSERT(res == 0);
    Py_XDECREF(path_str);
}

void ScriptRuntime::RunFile(const std::string& path) {
    PyEval_RestoreThread(tstate_);
    File f(path, "r");
    PyRun_SimpleFileExFlags(f.file(), path.c_str(), 0, nullptr);
    tstate_ = PyEval_SaveThread();
}

void ScriptRuntime::RunStr(const std::string& str) {
    PyEval_RestoreThread(tstate_);
    PyRun_SimpleString(str.c_str());
    tstate_ = PyEval_SaveThread();
}

}  // namespace charxed
