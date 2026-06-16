#pragma once

#include <functional>
#include <memory>
#include <string>

#include "result.h"
#include "state.h"
#include "term.h"
#include "utils.h"

namespace charxed {

struct Keyseq {
    std::string name;
    std::string description;
    int type;
    std::function<void()> f;

    Keyseq() = default;
    Keyseq(std::function<void()> _f, int _type = 0) : type(_type), f(_f) {}
};

class KeyseqManager {
   public:
    KeyseqManager(Mode& mode, Context& context);
    ~KeyseqManager() = default;  // TODO: implement it
    CHX_DELETE_COPY(KeyseqManager);
    CHX_DELETE_MOVE(KeyseqManager);

    // Add/Remove a keyseq, a keyseq is a key sequence for triggering a defined
    // handler. a keyseq prefixed with another keyseq will be hidden.
    // only ascii charset seq is supported. use \\ to escape < and >

    // throws KeyNotPredefinedException if key str is not pre-defined
    // this is always considered a bug and should fixed emidiately
    // return kError if keymap is not well formed
    Result AddKeyseq(const std::string& seq, const Keyseq& handler,
                     const std::vector<Mode>& modes = {CHX_DEFAULT_MODES},
                     const std::vector<Context>& contexts = {
                         CHX_DEFAULT_CONTEXTS});
    Result RemoveKeyseq(const std::string& seq,
                        const std::vector<Mode>& modes = {CHX_DEFAULT_MODES},
                        const std::vector<Context>& contexts = {
                            CHX_DEFAULT_CONTEXTS});

    // return kKeyseqError, kKeyseqDone, kKeyseqMatched
    // if kKeyseqDone return, handler will be set to the related handler
    // NOTE: Be careful of the handler lifetime
    Result FeedKey(const Terminal::KeyInfo& key, Keyseq*& handler);

   private:
    struct Node;
    using Nexts = std::unordered_map<size_t, Node*>;
    // use Trie tree to organize keymaps
    struct Node {
        std::unique_ptr<Keyseq> handler;
        Nexts nexts;
        Node* any_codepoint_next =
            nullptr;  // If key seq have <any-cp>, this will be set
        bool end = false;

        Node() = default;
        explicit Node(bool _end) : end(_end) {}
    };

   private:
    using KeymapsTrees =
        std::array<Node,
                   static_cast<size_t>(Mode::_kCount)>;  // one tree per mode

    std::array<KeymapsTrees, static_cast<size_t>(Context::_kCount)> roots_;

    // keyseq state
    Node* cur_ = nullptr;
    Mode last_mode_;
    Context last_context_;

    Mode& mode_;
    Context& context_;
};

}  // namespace charxed
