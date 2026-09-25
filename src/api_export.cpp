#include <Python.h>

#include "editor.h"
#include "script_runtime.h"

namespace charxed {

namespace {

PyObject* Notify(PyObject*, PyObject* args) {
    const char* text = nullptr;
    if (!PyArg_ParseTuple(args, "s", &text)) return nullptr;
    try {
        Editor::GetInstance().Notify(text);
        Py_RETURN_NONE;
    } catch (std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

PyObject* NotifyByStream(PyObject*, PyObject* args) {
    const char* text = nullptr;
    if (!PyArg_ParseTuple(args, "s", &text)) return nullptr;
    try {
        Editor::GetInstance().NotifyByStream(text);
        Py_RETURN_NONE;
    } catch (std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

PyMODINIT_FUNC PyInit_chx() {
    static PyMethodDef kMethods[] = {
        {"_notify_by_stream", NotifyByStream, METH_VARARGS, nullptr},
        {"notify", Notify, METH_VARARGS, nullptr},
        {nullptr, nullptr, 0, nullptr},
    };
    static PyModuleDef kModule = {PyModuleDef_HEAD_INIT,
                                  "_chx",
                                  "Charxed editor API",
                                  -1,
                                  kMethods,
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  nullptr};
    return PyModule_Create(&kModule);
}

}  // namespace

void ScriptRuntime::RegisterApi() {
    PyImport_AppendInittab("_chx", &PyInit_chx);
}

}  // namespace charxed
