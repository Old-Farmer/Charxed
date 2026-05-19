#include "signal_handler.h"

#include <cerrno>
#include <cstring>

#include "exception.h"
#include "signal.h"
#include "term.h"

namespace charxed {

sig_atomic_t sig_flag = SignalHandler::kNone;

static constexpr int kBadSignals[] = {SIGABRT, SIGSEGV, SIGBUS};
static constexpr int kGoodSignals[] = {SIGTSTP, SIGCONT};

// throw SignalRegisterException
static sighandler_t Signal(int signum, sighandler_t handler) {
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);  // not block any sigs
    action.sa_flags = SA_RESTART;  // restart syscalls if possible

    if (sigaction(signum, &action, &old_action) < 0) {
        throw SignalRegisterException("{}", strerror(errno));
    }
    return (old_action.sa_handler);
}

static void BadSignalHandler(int signum) {
    Terminal::GetInstance().Shutdown();
    try {
        Signal(signum, SIG_DFL);
    } catch (...) {
    }
    raise(signum);
}

static void GoodSignalHandler(int signum) {
    if (signum == SIGTSTP) {
        sig_flag = SignalHandler::kTStp;
    } else if (signum == SIGCONT) {
        sig_flag = SignalHandler::kCont;
    }
}

SignalHandler::SignalHandler() {}

void SignalHandler::Init() {
    for (int signum : kBadSignals) {
        Signal(signum, BadSignalHandler);
    }
    for (int signum : kGoodSignals) {
        Signal(signum, GoodSignalHandler);
    }
}
void SignalHandler::StopHandleTSTP() { Signal(SIGTSTP, SIG_DFL); }
void SignalHandler::StartHandleTSTP() { Signal(SIGTSTP, GoodSignalHandler); }

SignalHandler& SignalHandler::GetInstance() {
    static SignalHandler handler;
    return handler;
}

}  // namespace charxed
