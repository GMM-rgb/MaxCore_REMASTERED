// =========================================================================
// task_scheduler.hpp
// =========================================================================
// Native scheduler core: a timer thread, a worker pool, and a thread-safe
// completion queue. No Lua in this file, so it can be tested in isolation.
//
// THREADS
//   timer thread    owns the deadline heap. Sleeps with wait_until() to the
//                   nearest deadline, wakes early if a sooner one is added.
//                   Does no user work, so a slow task can never delay it.
//   worker threads  run native jobs (file loads, decoding, mesh prep).
//                   A job never touches a Lua state.
//   main thread     calls Drain() once per frame to collect finished work
//                   and due timers, then runs Lua callbacks itself.
//
// WHY THIS HOLDS UP UNDER FULL CPU LOAD
//   - The timer thread blocks in the kernel until a deadline. It uses no
//     CPU while waiting and gets scheduled promptly when it wakes.
//   - Deadlines are absolute (steady_clock time points), so being woken
//     late never makes later deadlines drift.
//   - Drain() has a time budget: it stops handing out work once the budget
//     is spent, so a backlog can't stretch a frame.
//   - FrameClock turns "elapsed since last frame" into a bounded number of
//     fixed steps and DROPS the rest, so a starved main thread recovers
//     in one frame instead of spiralling into ever-larger catch-up work.
//
// MEMORY SAFETY
//   - Every task is owned by the scheduler in a slot table. Callers hold a
//     Handle {index, generation}. Cancel/destroy bumps the generation, so a
//     stale Handle resolves to "nothing" instead of dangling.
//   - Shutdown sets a stop flag, wakes everyone, and joins every thread
//     before any member is destroyed. No detached threads.
//   - No raw new/delete. Containers and unique_ptr only.
// =========================================================================
#ifndef TASK_SCHEDULER_HPP
#define TASK_SCHEDULER_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
    #include <pthread.h>
    #include <sched.h>
#endif

namespace Sched {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Seconds = std::chrono::duration<double>;

inline double ToSeconds(TimePoint t, TimePoint origin) {
    return std::chrono::duration<double>(t - origin).count();
}

// -------------------------------------------------------------------------
// Handle: index + generation. Stale handles are detected, never followed.
// -------------------------------------------------------------------------
struct Handle {
    uint32_t index = 0;
    uint32_t generation = 0; // 0 is never a valid generation
    bool Valid() const { return generation != 0; }
    uint64_t Pack() const { return ((uint64_t)generation << 32) | index; }
    static Handle Unpack(uint64_t v) { return Handle{(uint32_t)(v & 0xFFFFFFFFu), (uint32_t)(v >> 32)}; }
    bool operator==(const Handle& o) const { return index == o.index && generation == o.generation; }
};

// What the main thread gets back from Drain().
struct Completion {
    enum class Kind { Timer, Work } kind = Kind::Timer;
    Handle handle;           // the task that finished or fired
    bool ok = true;          // Work only: false if the job failed or threw
    std::string error;       // Work only: failure text when ok == false
    uint64_t result = 0;     // Work only: opaque number the job returned
    std::string payload;     // Work only: bytes the job produced (e.g. a file's contents).
                             // Travels INSIDE the completion, so a cancelled task's
                             // payload is freed with it: there is no side table to leak.
};

// What a background job returns.
struct JobResult {
    bool ok = true;
    uint64_t number = 0;
    std::string payload;
    std::string error;
    JobResult() = default;
    JobResult(uint64_t n) : number(n) {}   // lets simple jobs keep writing `return 42;`
};

// Best-effort: ask the OS to schedule this thread ahead of ordinary work.
// Never required for correctness; failure is silent and harmless.
inline bool RaiseCurrentThreadPriority() {
#if defined(_WIN32) || defined(_WIN64)
    return SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL) != 0;
#elif defined(__linux__) || defined(__APPLE__)
    sched_param sp{};
    int policy = 0;
    if (pthread_getschedparam(pthread_self(), &policy, &sp) != 0) return false;
    int maxP = sched_get_priority_max(policy);
    int minP = sched_get_priority_min(policy);
    if (maxP < 0 || minP < 0 || maxP == minP) return false; // SCHED_OTHER has one level on Linux; that's fine
    sp.sched_priority = std::min(maxP, sp.sched_priority + 1);
    return pthread_setschedparam(pthread_self(), policy, &sp) == 0;
#else
    return false;
#endif
}

// -------------------------------------------------------------------------
// Scheduler
// -------------------------------------------------------------------------
class Scheduler {
public:
    using Job = std::function<JobResult()>; // runs on a worker; never touches a Lua state

    explicit Scheduler(unsigned workerCount = 0) : origin_(Clock::now()) {
        if (workerCount == 0) {
            // Leave headroom for the main thread (running Lua and the frame loop)
            // and the timer thread. Workers that use every core compete with the
            // heartbeat under load, which is the exact thing this scheduler is
            // meant to prevent. Never fewer than one worker.
            unsigned hw = std::thread::hardware_concurrency();
            workerCount = hw > 3 ? hw - 2 : 1;
        }
        timer_ = std::thread([this] { TimerLoop(); });
        workers_.reserve(workerCount);
        for (unsigned i = 0; i < workerCount; ++i) {
            workers_.emplace_back([this] { WorkerLoop(); });
        }
    }

    ~Scheduler() { Shutdown(); }

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    // Stops everything and joins all threads. Safe to call twice.
    void Shutdown() {
        {
            std::lock_guard<std::mutex> lk(timerMu_);
            if (stop_.exchange(true)) {
                // already stopping/stopped; still make sure threads are joined below
            }
        }
        timerCv_.notify_all();
        {
            std::lock_guard<std::mutex> lk(workMu_);
        }
        workCv_.notify_all();
        {
            std::lock_guard<std::mutex> lk(doneMu_);
        }
        doneCv_.notify_all(); // a host blocked in WaitForCompletion must wake on shutdown
        if (timer_.joinable()) timer_.join();
        for (auto& w : workers_) if (w.joinable()) w.join();
        workers_.clear();
    }

    double Now() const { return ToSeconds(Clock::now(), origin_); }

    // ---- timers ---------------------------------------------------------
    // Fires once after `delaySeconds`. Returns a handle usable with Cancel.
    Handle AddTimer(double delaySeconds) {
        if (!(delaySeconds > 0.0)) delaySeconds = 0.0; // NaN/negative -> now
        if (delaySeconds > 86400.0) delaySeconds = 86400.0;
        Handle h = AllocSlot();
        TimePoint due = Clock::now() + std::chrono::duration_cast<Clock::duration>(Seconds(delaySeconds));
        {
            std::lock_guard<std::mutex> lk(timerMu_);
            if (stop_) { FreeSlot(h); return Handle{}; }
            heap_.push(Entry{due, h});
        }
        timerCv_.notify_one(); // the new deadline may be sooner than the one the timer is sleeping toward
        return h;
    }

    // ---- background work ------------------------------------------------
    Handle AddWork(Job job) {
        Handle h = AllocSlot();
        {
            std::lock_guard<std::mutex> lk(workMu_);
            if (stop_) { FreeSlot(h); return Handle{}; }
            workQueue_.push_back(WorkItem{h, std::move(job)});
        }
        workCv_.notify_one();
        return h;
    }

    // Prevents the completion from being delivered. Returns true if the task
    // was still pending. Work already running finishes, but its result is
    // discarded because the generation no longer matches.
    bool Cancel(Handle h) {
        return FreeSlot(h);
    }

    bool IsPending(Handle h) const {
        std::lock_guard<std::mutex> lk(slotMu_);
        return h.index < slots_.size() && slots_[h.index].generation == h.generation && slots_[h.index].live;
    }

    // ---- main thread ----------------------------------------------------
    // Collects finished work and due timers. Stops early once `budget` is
    // spent (checked between items) so a backlog can't stretch a frame.
    // Returns how many completions were appended to `out`.
    size_t Drain(std::vector<Completion>& out, double budgetSeconds = 0.002, size_t maxItems = 4096) {
        size_t n = 0;
        TimePoint start = Clock::now();
        std::deque<Completion> local;
        {
            std::lock_guard<std::mutex> lk(doneMu_);
            // Take only as many as we are willing to hand out this frame.
            while (!done_.empty() && local.size() < maxItems) {
                local.push_back(std::move(done_.front()));
                done_.pop_front();
            }
        }
        while (!local.empty()) {
            Completion c = std::move(local.front());
            local.pop_front();
            // Deliver only if the handle is still the live owner of its slot.
            if (!ConsumeSlot(c.handle)) continue; // cancelled while in flight
            out.push_back(std::move(c));
            ++n;
            if (std::chrono::duration<double>(Clock::now() - start).count() >= budgetSeconds && !local.empty()) {
                // Out of budget: put the rest back at the FRONT, order preserved.
                std::lock_guard<std::mutex> lk(doneMu_);
                while (!local.empty()) {
                    done_.push_front(std::move(local.back()));
                    local.pop_back();
                }
                break;
            }
        }
        return n;
    }

    // Seconds until the next timer deadline (or `fallback` if none). Lets the
    // main loop sleep exactly as long as it safely can.
    double SecondsUntilNextTimer(double fallback) {
        std::lock_guard<std::mutex> lk(timerMu_);
        PruneDeadTop();                       // a cancelled timer must not keep the host waking up
        if (heap_.empty()) return fallback;
        double s = std::chrono::duration<double>(heap_.top().due - Clock::now()).count();
        return s < 0 ? 0 : s;
    }

    // Blocks the calling thread until a completion is ready or `maxSeconds`
    // passes, whichever is first. This is what lets a host sleep through
    // idle time AND still wake the moment a worker finishes.
    void WaitForCompletion(double maxSeconds) {
        if (maxSeconds <= 0) return;
        std::unique_lock<std::mutex> lk(doneMu_);
        doneCv_.wait_for(lk, std::chrono::duration_cast<Clock::duration>(Seconds(maxSeconds)),
                         [this] { return !done_.empty() || stop_.load(); });
    }

    size_t PendingCount() const {
        std::lock_guard<std::mutex> lk(slotMu_);
        size_t n = 0;
        for (const auto& s : slots_) if (s.live) ++n;
        return n;
    }
    size_t WorkerCount() const { return workers_.size(); }

private:
    struct Entry {
        TimePoint due;
        Handle handle;
        bool operator>(const Entry& o) const { return due > o.due; }
    };
    struct WorkItem {
        Handle handle;
        Job job;
    };
    struct Slot {
        uint32_t generation = 1;
        bool live = false;
    };

    // ---- slot table: generation-checked ownership -----------------------
    Handle AllocSlot() {
        std::lock_guard<std::mutex> lk(slotMu_);
        uint32_t idx;
        if (!freeList_.empty()) {
            idx = freeList_.back();
            freeList_.pop_back();
        } else {
            idx = (uint32_t)slots_.size();
            slots_.push_back(Slot{});
        }
        slots_[idx].live = true;
        return Handle{idx, slots_[idx].generation};
    }

    // Frees a slot if `h` still owns it. Bumping the generation is what
    // invalidates every copy of the handle.
    bool FreeSlot(Handle h) {
        std::lock_guard<std::mutex> lk(slotMu_);
        if (h.index >= slots_.size()) return false;
        Slot& s = slots_[h.index];
        if (!s.live || s.generation != h.generation) return false;
        s.live = false;
        s.generation = (s.generation == 0xFFFFFFFFu) ? 1u : s.generation + 1u; // never 0
        freeList_.push_back(h.index);
        return true;
    }

    // Same as FreeSlot: delivering a completion consumes the task.
    bool ConsumeSlot(Handle h) { return FreeSlot(h); }

    bool SlotLive(Handle h) const {
        std::lock_guard<std::mutex> lk(slotMu_);
        return h.index < slots_.size() && slots_[h.index].live && slots_[h.index].generation == h.generation;
    }

    // Pops cancelled entries off the top of the deadline heap. A priority
    // queue can't remove from the middle, so cancelled timers are discarded
    // lazily here instead. LOCK ORDER: caller holds timerMu_; this takes
    // slotMu_ (via SlotLive). Nothing takes them in the opposite order.
    void PruneDeadTop() {
        while (!heap_.empty() && !SlotLive(heap_.top().handle)) heap_.pop();
    }

    // ---- timer thread ---------------------------------------------------
    void TimerLoop() {
        RaiseCurrentThreadPriority();
        std::unique_lock<std::mutex> lk(timerMu_);
        while (!stop_) {
            PruneDeadTop();                   // don't sleep toward a deadline nobody wants anymore
            if (heap_.empty()) {
                timerCv_.wait(lk, [this] { return stop_.load() || !heap_.empty(); });
                continue;
            }
            TimePoint due = heap_.top().due;
            if (Clock::now() < due) {
                // Sleeps to the deadline, or wakes early if a sooner timer is
                // added (AddTimer notifies) or shutdown begins.
                timerCv_.wait_until(lk, due);
                continue;
            }
            // Collect everything that is due now in one pass, then publish
            // outside the heap lock so producers aren't blocked on us.
            std::vector<Handle> fired;
            TimePoint now = Clock::now();
            while (!heap_.empty() && heap_.top().due <= now) {
                fired.push_back(heap_.top().handle);
                heap_.pop();
            }
            lk.unlock();
            for (Handle h : fired) {
                if (!SlotLive(h)) continue; // cancelled before it fired
                Completion c;
                c.kind = Completion::Kind::Timer;
                c.handle = h;
                PushDone(std::move(c));
            }
            lk.lock();
        }
    }

    // ---- worker threads -------------------------------------------------
    void WorkerLoop() {
        for (;;) {
            WorkItem item;
            {
                std::unique_lock<std::mutex> lk(workMu_);
                workCv_.wait(lk, [this] { return stop_.load() || !workQueue_.empty(); });
                if (stop_ && workQueue_.empty()) return;
                item = std::move(workQueue_.front());
                workQueue_.pop_front();
            }
            if (!SlotLive(item.handle)) continue; // cancelled before it started

            Completion c;
            c.kind = Completion::Kind::Work;
            c.handle = item.handle;
            try {
                if (item.job) {
                    JobResult r = item.job();
                    c.ok = r.ok;
                    c.result = r.number;
                    c.payload = std::move(r.payload);
                    c.error = std::move(r.error);
                }
            } catch (const std::exception& e) {
                c.ok = false;
                c.error = e.what();
            } catch (...) {
                c.ok = false;
                c.error = "unknown exception in background job";
            }
            PushDone(std::move(c));
        }
    }

    void PushDone(Completion&& c) {
        {
            std::lock_guard<std::mutex> lk(doneMu_);
            done_.push_back(std::move(c));
        }
        doneCv_.notify_one();
    }

    // ---- state ----------------------------------------------------------
    const TimePoint origin_;
    std::atomic<bool> stop_{false};

    mutable std::mutex timerMu_;
    std::condition_variable timerCv_;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> heap_;
    std::thread timer_;

    std::mutex workMu_;
    std::condition_variable workCv_;
    std::deque<WorkItem> workQueue_;
    std::vector<std::thread> workers_;

    mutable std::mutex doneMu_;
    std::condition_variable doneCv_;
    std::deque<Completion> done_;

    mutable std::mutex slotMu_;
    std::vector<Slot> slots_;
    std::vector<uint32_t> freeList_;
};

// -------------------------------------------------------------------------
// FrameClock: fixed-timestep with bounded catch-up.
// -------------------------------------------------------------------------
// Call Advance(elapsed) once per loop pass. It returns how many fixed steps
// to run (0..maxSteps). Whatever time can't be spent in those steps is
// DROPPED rather than carried, which is what stops the classic "spiral of
// death": a starved frame normally causes a big catch-up, the catch-up makes
// the next frame slower, and so on. Here a stall costs at most maxSteps of
// work once and the clock resyncs.
class FrameClock {
public:
    explicit FrameClock(double stepSeconds, int maxSteps = 3) : step_(stepSeconds), maxSteps_(maxSteps) {}

    int Advance(double elapsed) {
        if (!(elapsed > 0)) return 0;
        acc_ += elapsed;
        int steps = (int)(acc_ / step_);
        if (steps > maxSteps_) {
            dropped_ += acc_ - maxSteps_ * step_;
            steps = maxSteps_;
            acc_ = 0; // resync: don't carry the backlog
        } else {
            acc_ -= steps * step_;
        }
        return steps;
    }

    double Step() const { return step_; }
    double Accumulated() const { return acc_; }
    double DroppedSeconds() const { return dropped_; }
    // Seconds until the next step is due (0 if one is already due).
    double UntilNext() const { double r = step_ - acc_; return r > 0 ? r : 0; }

private:
    double step_;
    int maxSteps_;
    double acc_ = 0;
    double dropped_ = 0;
};

} // namespace Sched

#endif // TASK_SCHEDULER_HPP
