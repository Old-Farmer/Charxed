#pragma once

#include <csetjmp>
#include <csignal>

namespace charxed {

extern sig_atomic_t sig_flag;

class SignalHandler {
    // Register a handler of some known signals for recovering the terminal
    // state back to normal when the program receives some signals and quits,
    // especially for bugs or SIGSTOP
    // Best effort.
    // TODO: do it better
    SignalHandler();

   public:
    static SignalHandler& GetInstance();

    void Init();
    void StopHandleTSTP();
    void StartHandleTSTP();

    static constexpr sig_atomic_t kNone = 0;
    static constexpr sig_atomic_t kTStp = 1;
    static constexpr sig_atomic_t kCont = 1;
};

}  // namespace charxed
