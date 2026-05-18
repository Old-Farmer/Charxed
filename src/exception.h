#pragma once

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <utility>

#include "fmt/format.h"

namespace charxed {

// Print an exception info.
// if e != nullptr, the type of e and e.what() will be shown;
// else, only show the current exception's type.
// NOTE: This function should only be called in terminate handler.
void PrintException(const std::exception* e);

class Exception : public std::exception {
   public:
    Exception() = default;
    template <typename... Args>
    Exception(const char* format, Args&&... args) {
        msg_ = fmt::format(format, std::forward<Args>(args)...);
    }

    virtual const char* what() const noexcept override { return msg_.c_str(); }

   protected:
    std::string msg_;
};

#define CHX_NORMAL_EXCEPTION(exception)                         \
    class exception : public Exception {                        \
       public:                                                  \
        template <typename... Args>                             \
        exception(const char* format, Args&&... args)           \
            : Exception(format, std::forward<Args>(args)...) {} \
    };

CHX_NORMAL_EXCEPTION(TermException)
CHX_NORMAL_EXCEPTION(IOException)
CHX_NORMAL_EXCEPTION(FileCreateException)
CHX_NORMAL_EXCEPTION(CodingException)
CHX_NORMAL_EXCEPTION(FSException)
CHX_NORMAL_EXCEPTION(LogInitException)
CHX_NORMAL_EXCEPTION(SignalRegisterException)
CHX_NORMAL_EXCEPTION(KeyNotPredefinedException)
CHX_NORMAL_EXCEPTION(TSQueryPredicateDirectiveNotSupportException)
CHX_NORMAL_EXCEPTION(RegexCompileException)

class OSException : public Exception {
   public:
    template <typename... Args>
    OSException(int error_code, const char* format, Args&&... args)
        : Exception(format, std::forward<Args>(args)...),
          error_code_(error_code) {}
    int error_code() { return error_code_; }

   private:
    int error_code_;
};

CHX_NORMAL_EXCEPTION(TypeMismatchException)
CHX_NORMAL_EXCEPTION(OptionLoadException)
CHX_NORMAL_EXCEPTION(ParseMsgException)
CHX_NORMAL_EXCEPTION(CommandNameExistException)
CHX_NORMAL_EXCEPTION(OptionInfoInitException)
}  // namespace charxed
