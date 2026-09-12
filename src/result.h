#pragma once

#include <cstdio>
#include <cstdlib>

#include "fmt/core.h"

namespace charxed {

#define CHX_RESULT_TABLE                                                       \
    X(kOk, "Ok")                                                               \
    X(kError, "Error")                                                         \
    X(kFail, "Fail") /*A failure is not an error. A failure means we     can't \
                        do sth. */                                             \
    X(kNotExist, "Not Exist")                                                  \
    X(kInvalidCoding, "Invalid Encoding")                                      \
    X(kEof, "End of File")                                                     \
    X(kBufferNoBackupFile, "Buffer no backup file")                            \
    X(kBufferCannotLoad, "Buffer can't be loaded")                             \
    X(kBufferReadOnly, "Buffer readOnly")                                      \
    X(kKeyseqError, "Key sequence not valid")                                  \
    X(kKeyseqDone, "Key sequence fully matched")                               \
    X(kKeyseqMatched, "Key sequence partially matched")                        \
    X(kCommandInvalidArgs, "Command call has invalid args")                    \
    X(kCommandEmpty, "Command input str empty")                                \
    X(kNoHistoryAvailable, "No history availabe before/after the cursor")      \
    X(kOuterCommandExecuteFail, "Outer command fails before or when execvp")   \
    X(kRetriggerCmp, "Please retrigger the completion")                        \
    X(kMsgNeedMore, "Need recv more for parsing a complete msg")               \
    X(kWrapHistory, "Buffer's history wrapped")                                \
    X(kSelectionStarted, "Selection started")                                  \
    X(kSearchPatternOnly, "Input only search pattern")                         \
    X(kSearchPatternWithReplace,                                               \
      "Input contains search pattern & replace "                               \
      "str")                                                                   \
    X(kReplaceStrNotAvailable, "Replace str is not availabe")

enum Result {
#define X(result, str) result,
    CHX_RESULT_TABLE
#undef X
};

inline const char* ResultString(Result result) {
    switch (result) {
#define X(res, str) \
    case res:       \
        return str;
        CHX_RESULT_TABLE
    };
#undef X
    return ""; // make compiler happy
}

}  // namespace charxed
