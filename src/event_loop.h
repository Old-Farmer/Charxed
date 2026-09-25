#pragma once

#include <functional>
#include <thread>
#include <unordered_map>
#include <vector>

#include "os.h"
#include "poll.h"
#include "timer_manager.h"

namespace charxed {

constexpr int kEventRead = 0b1;
constexpr int kEventWrite = 0b10;
constexpr int kEventClose = 0b100;
constexpr int kEventError = 0b1000;

using EventFD = int;
using Event = int;
using EventHandler = std::function<void(Event)>;

struct EventInfo {
    EventFD fd;
    Event Interesting_events = 0;
    EventHandler handler;
};

class GlobalOpts;

// Event loop will handle handler events.
class EventLoop {
   public:
    // throw OSException
    EventLoop(GlobalOpts* global_opts);
    ~EventLoop();

    void AddEventHandler(const EventInfo& info);
    void RemoveEventHandler(EventFD fd);

    void BeforePoll(const std::function<void()>& f);
    void AfterAllEvents(const std::function<void()>& f);

    void Loop();
    void EndLoop() { quit_ = true; }

    // If not in the same thread with loop, this method will wake up the main
    // loop if it's waiting for events.
    // thread safe
    void WakeUpLoop();

   private:
    std::vector<pollfd> poll_fds_;

    std::unordered_map<EventFD, EventInfo> event_infos_;
    bool changed_ = false;
    bool quit_ = false;

    std::function<void()> before_poll_;
    std::function<void()> after_all_events_;

    std::thread::id loop_thread_id_;
    Fd wakeup_fd_[2];

    GlobalOpts* global_opts_;

   public:
    TimerManager timer_manager_;
};

}  // namespace charxed
