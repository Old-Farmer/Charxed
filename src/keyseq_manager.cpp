#include "keyseq_manager.h"

#include <stack>

#include "character.h"

namespace charxed {

namespace {

using tm = Terminal;
using ki = Terminal::KeyInfo;
using sk = Terminal::SpecialKey;

// A special wrapper of KeyInfo
// It can represent two state:
// 1. specific key
// 2. any codepoint key
struct KeyInfoOrAnyCodepoint {
    tm::KeyInfo key_info;
    bool any_codepoint;

    // Specific key
    KeyInfoOrAnyCodepoint(tm::KeyInfo _key_info)
        : key_info(_key_info), any_codepoint(false) {}

    // Any code point
    KeyInfoOrAnyCodepoint() : any_codepoint(true) {}
};

// only support ascii keystr
const std::unordered_map<std::string_view, KeyInfoOrAnyCodepoint>
    kKeyStrToKeyInfo = {
        // Any code point
        {"<any-cp>", {}},
        // esc
        {"<esc>", {ki::CreateSpecialKey(sk::kEsc)}},
        {"<c-[>", {ki::CreateSpecialKey(sk::kCtrlLsqBracket)}},  // same <esc>
        {"<c-3>", {ki::CreateSpecialKey(sk::kCtrl3)}},           // same <esc>

        // traditioncal ascii control code
        {"<c-~>", {ki::CreateSpecialKey(sk::kCtrlTilde, tm::kCtrl)}},
        {"<c-space>",
         {ki::CreateSpecialKey(
             sk::kCtrlTilde,
             tm::kCtrl)}},  // NOTE:
                            // 1. In Windows Terminal, same as
                            // <c-~>; 2. in alacritty, no effect.
                            // TODO: maybe some detection?

        {"<c-a>", {ki::CreateSpecialKey(sk::kCtrlA, tm::kCtrl)}},
        {"<c-b>", {ki::CreateSpecialKey(sk::kCtrlB, tm::kCtrl)}},
        {"<c-c>", {ki::CreateSpecialKey(sk::kCtrlC, tm::kCtrl)}},
        {"<c-d>", {ki::CreateSpecialKey(sk::kCtrlD, tm::kCtrl)}},
        {"<c-e>", {ki::CreateSpecialKey(sk::kCtrlE, tm::kCtrl)}},
        {"<c-f>", {ki::CreateSpecialKey(sk::kCtrlF, tm::kCtrl)}},
        {"<c-g>", {ki::CreateSpecialKey(sk::kCtrlG, tm::kCtrl)}},
        {"<c-i>", {ki::CreateSpecialKey(sk::kCtrlI, tm::kCtrl)}},
        {"<tab>", {ki::CreateSpecialKey(sk::kTab, tm::kCtrl)}},  // same <c-i>
        {"<c-j>", {ki::CreateSpecialKey(sk::kCtrlJ, tm::kCtrl)}},
        {"<c-k>", {ki::CreateSpecialKey(sk::kCtrlK, tm::kCtrl)}},
        {"<c-l>", {ki::CreateSpecialKey(sk::kCtrlL, tm::kCtrl)}},
        {"<enter>", {ki::CreateSpecialKey(sk::kEnter, tm::kCtrl)}},
        {"<c-m>",
         {ki::CreateSpecialKey(sk::kCtrlM, tm::kCtrl)}},  // same <enter>
        {"<c-n>", {ki::CreateSpecialKey(sk::kCtrlN, tm::kCtrl)}},
        {"<c-o>", {ki::CreateSpecialKey(sk::kCtrlO, tm::kCtrl)}},
        {"<c-p>", {ki::CreateSpecialKey(sk::kCtrlP, tm::kCtrl)}},
        {"<c-q>", {ki::CreateSpecialKey(sk::kCtrlQ, tm::kCtrl)}},
        {"<c-r>", {ki::CreateSpecialKey(sk::kCtrlR, tm::kCtrl)}},
        {"<c-s>", {ki::CreateSpecialKey(sk::kCtrlS, tm::kCtrl)}},
        {"<c-u>", {ki::CreateSpecialKey(sk::kCtrlU, tm::kCtrl)}},
        {"<c-v>", {ki::CreateSpecialKey(sk::kCtrlV, tm::kCtrl)}},
        {"<c-w>", {ki::CreateSpecialKey(sk::kCtrlW, tm::kCtrl)}},
        {"<c-x>", {ki::CreateSpecialKey(sk::kCtrlX, tm::kCtrl)}},
        {"<c-y>", {ki::CreateSpecialKey(sk::kCtrlY, tm::kCtrl)}},
        {"<c-z>", {ki::CreateSpecialKey(sk::kCtrlZ, tm::kCtrl)}},
        {"<bs>", {ki::CreateSpecialKey(sk::kBackspace2, tm::kCtrl)}},
        {"<c-8>", {ki::CreateSpecialKey(sk::kCtrl8, tm::kCtrl)}},  // same <bs>
        {"<s-tab>", {ki::CreateSpecialKey(sk::kBackTab, tm::kCtrl)}},
        {"<space>", {ki::CreateNormalKey(kSpaceChar)}},

        // functional key
        {"<up>", {ki::CreateSpecialKey(sk::kArrowUp)}},
        {"<down>", {ki::CreateSpecialKey(sk::kArrowDown)}},
        {"<left>", {ki::CreateSpecialKey(sk::kArrowLeft)}},
        {"<right>", {ki::CreateSpecialKey(sk::kArrowRight)}},
        {"<c-left>", {ki::CreateSpecialKey(sk::kArrowLeft, tm::kCtrl)}},
        {"<c-right>", {ki::CreateSpecialKey(sk::kArrowRight, tm::kCtrl)}},
        {"<a-left>", {ki::CreateSpecialKey(sk::kArrowLeft, tm::kAlt)}},
        {"<a-right>", {ki::CreateSpecialKey(sk::kArrowRight, tm::kAlt)}},
        {"<home>", {ki::CreateSpecialKey(sk::kHome)}},
        {"<end>", {ki::CreateSpecialKey(sk::kEnd)}},
        {"<pgup>", {ki::CreateSpecialKey(sk::kPgup)}},
        {"<pgdn>", {ki::CreateSpecialKey(sk::kPgdn)}},
        {"<c-pgup>", {ki::CreateSpecialKey(sk::kPgup, tm::kCtrl)}},
        {"<c-pgdn>", {ki::CreateSpecialKey(sk::kPgdn, tm::kCtrl)}},
};

Result ParseKeyseq(const std::string& seq,
                   std::vector<KeyInfoOrAnyCodepoint>& keys) {
    int start = -1;
    for (size_t i = 0; i < seq.size(); i++) {
        if (seq[i] == '\\') {
            if (i == seq.size() - 1) {
                throw DanglingEscapeException("Dangling esc at {}", seq);
            }
            i++;
            keys.push_back(Terminal::KeyInfo::CreateNormalKey(seq[i]));
        } else if (seq[i] == '<') {
            if (start != -1) {
                return kError;
            }
            start = i;
        } else if (seq[i] == '>') {
            if (start == -1) {
                return kError;
            }

            std::string_view key = {seq.c_str() + start, i - start + 1};
            auto iter = kKeyStrToKeyInfo.find(key);
            if (iter == kKeyStrToKeyInfo.end()) {
                std::string key_copy(key);
                throw KeyNotPredefinedException("Didn't predefine {}",
                                                key_copy.c_str());
            }
            keys.push_back(iter->second);
            start = -1;
        } else if (start == -1) {
            keys.push_back(Terminal::KeyInfo::CreateNormalKey(seq[i]));
        }
    }
    return kOk;
}

}  // namespace

KeyseqManager::KeyseqManager(Mode& mode, Context& context)
    : mode_(mode), context_(context) {
    for (auto& roots_in_context : roots_) {
        for (auto& root : roots_in_context) {
            root = Node(true);
        }
    }
}

Result KeyseqManager::AddKeyseq(const std::string& seq, const Keyseq& handler,
                                const std::vector<Mode>& modes,
                                const std::vector<Context>& contexts) {
    std::vector<KeyInfoOrAnyCodepoint> keys;
    Result res = ParseKeyseq(seq, keys);
    if (res != kOk) {
        return res;
    }
    for (auto context : contexts) {
        for (auto mode : modes) {
            Node* node =
                &roots_[static_cast<int>(context)][static_cast<int>(mode)];
            for (const auto& key : keys) {
                if (key.any_codepoint) {
                    if (node->any_codepoint_next == nullptr) {
                        node->any_codepoint_next = new Node;
                    }
                    node = node->any_codepoint_next;
                } else {
                    auto iter = node->nexts.find(key.key_info.ToNumber());
                    if (iter == node->nexts.end()) {
                        node->nexts[key.key_info.ToNumber()] = new Node;
                    }
                    node = node->nexts[key.key_info.ToNumber()];
                }
            }
            node->end = true;
            node->handler = std::make_unique<Keyseq>(handler);
        }
    }
    return kOk;
}

Result KeyseqManager::RemoveKeyseq(const std::string& seq,
                                   const std::vector<Mode>& modes,
                                   const std::vector<Context>& contexts) {
    std::vector<KeyInfoOrAnyCodepoint> keys;
    Result res = ParseKeyseq(seq, keys);
    if (res != kOk) {
        return res;
    }
    for (auto context : contexts) {
        for (Mode mode : modes) {
            Node* node =
                &roots_[static_cast<int>(context)][static_cast<int>(mode)];
            std::stack<std::pair<Node*, Nexts::iterator>> sta;
            bool to_end = true;  // We truly find this key seq in the tree?
            for (const auto& key : keys) {
                Nexts::iterator iter;
                if (key.any_codepoint) {
                    if (node->any_codepoint_next == nullptr) {
                        to_end = false;
                        break;
                    }
                    iter = node->nexts.end();  // Mark as end()
                } else {
                    iter = node->nexts.find(key.key_info.ToNumber());
                    if (iter == node->nexts.end()) {
                        to_end = false;
                        break;
                    }
                }
                sta.push({node, iter});
                node = iter->second;
            }
            if (!to_end || !node->end) {
                continue;
            }
            node->end = false;
            if (!node->nexts.empty()) {
                continue;
            }

            delete node;
            while (!sta.empty()) {
                auto [node, iter] = sta.top();
                if (iter == node->nexts.end()) {
                    node->any_codepoint_next = nullptr;
                } else {
                    node->nexts.erase(iter);
                }
                if (node->end) {
                    break;
                }
                if (!node->nexts.empty()) {
                    break;
                }

                if (node != &roots_[static_cast<int>(context)]
                                   [static_cast<int>(mode)]) {
                    delete node;
                }
                sta.pop();
            }
        }
    }
    return kOk;
}

Result KeyseqManager::FeedKey(const Terminal::KeyInfo& key, Keyseq*& handler) {
    if (cur_ == nullptr) {
        cur_ = &roots_[static_cast<int>(context_)][static_cast<int>(mode_)];
        last_mode_ = mode_;
        last_context_ = context_;
    } else if (last_mode_ != mode_ || last_context_ != context_) {
        cur_ = nullptr;
        return kKeyseqError;
    }

    // First try to find any specific key match,
    // If we can't find any match, we try any_codepoint_next.
    // This behavior is aligned to AddKeyseq and RemoveKeyseq.
    auto iter = cur_->nexts.find(key.ToNumber());
    if (iter == cur_->nexts.end()) {
        if (cur_->any_codepoint_next == nullptr || key.IsSpecialKey()) {
            cur_ = nullptr;
            return kKeyseqError;
        }
        cur_ = cur_->any_codepoint_next;
    } else {
        cur_ = iter->second;
    }

    if (cur_->end) {
        handler = cur_->handler.get();
        CHX_ASSERT(handler != nullptr);
        cur_ = nullptr;
        return kKeyseqDone;
    } else {
        return kKeyseqMatched;
    }
}

}  // namespace charxed
