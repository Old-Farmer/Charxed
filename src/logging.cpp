#include "logging.h"

#include <cstdio>
#include <cstring>

#include "exception.h"
#include "fs.h"

namespace charxed {
FILE* logging_file;

std::string GetLoggingFilePath() {
    return Path::GetXDGPath(XDGPath::kState) + "charxed.log";
}

void LogInit(const std::string& file) {
    logging_file = fopen(file.c_str(), "a+");
    if (logging_file == nullptr) {
        throw LogInitException("Log file can't open: {}", strerror(errno));
    }
}

void LogDeinit() { fclose(logging_file); }
}  // namespace charxed
