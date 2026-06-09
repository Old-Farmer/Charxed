#include "clipboard.h"

#include "constants.h"
#include "logging.h"
#include "subprocess.h"
#include "utf8.h"

namespace charxed {

std::unique_ptr<ClipBoard> ClipBoard::CreateClipBoard(bool prefer_system) {
    if (prefer_system) {
        if (XClipBoard::DetectUsable()) {
            return std::make_unique<XClipBoard>();
        }
        return std::make_unique<DefaultClipBoard>();
    } else {
        return std::make_unique<DefaultClipBoard>();
    }
}

std::string DefaultClipBoard::GetContent(bool& lines) const {
    lines = lines_;
    return content_;
}

void DefaultClipBoard::SetContent(const std::string& content, bool lines) {
    lines_ = lines;
    content_ = content;
}

void DefaultClipBoard::SetContent(std::string&& content, bool lines) {
    lines_ = lines;
    content_ = std::move(content);
}

bool XClipBoard::DetectUsable() {
    const char* const argv[] = {"xsel", "--help", nullptr};
    int exit_code;
    Result res = Exec(argv, nullptr, nullptr, nullptr, exit_code);
    return exit_code == 0 && res == kOk;
}

XClipBoard::XClipBoard() {
    const char* v = getenv(kWSLEnv);
    in_wsl_ = v != nullptr;
}

std::string XClipBoard::GetContent(bool& lines) const {
    lines = lines_;
    const char* const argv[] = {"xsel", "--clipboard", "--output", nullptr};
    std::string content;
    int exit_code;
    Result res = Exec(argv, nullptr, &content, nullptr, exit_code);
    if (exit_code != 0 || res != kOk) {
        CHX_LOG_ERROR("xsel --clipboard error, exit code: {}", exit_code);
        return "";
    }

    if (content.empty()) {
        return "";
    }

    if (in_wsl_) WslFilterCharacter(content);
    // Assume content is utf-8?
    // TODO: Really?
    if (!CheckUtf8Valid(content)) {
        CHX_LOG_ERROR("xsel content utf-8 invalid");
        return "";
    }
    return content;
}

void XClipBoard::SetContent(const std::string& content, bool lines) {
    lines_ = lines;
    const char* const argv[] = {"xsel", "--clipboard", "--input", nullptr};
    int exit_code;
    Result res = Exec(argv, &content, nullptr, nullptr, exit_code);
    if (exit_code != 0 || res != kOk) {
        return;
    }
}

void XClipBoard::SetContent(std::string&& content, bool lines) {
    lines_ = lines;
    const char* const argv[] = {"xsel", "--clipboard", "--input", nullptr};
    int exit_code;
    Result res = Exec(argv, &content, nullptr, nullptr, exit_code);
    if (exit_code != 0 || res != kOk) {
        return;
    }
}

void XClipBoard::WslFilterCharacter(std::string& content) {
    auto iter_end = content.end() - 1;
    for (auto iter = content.begin(); iter != iter_end;) {
        if (*iter == '\r' && *(iter + 1) == '\n') {
            iter = content.erase(iter);
        } else {
            iter++;
        }
    }
}

}  // namespace charxed
