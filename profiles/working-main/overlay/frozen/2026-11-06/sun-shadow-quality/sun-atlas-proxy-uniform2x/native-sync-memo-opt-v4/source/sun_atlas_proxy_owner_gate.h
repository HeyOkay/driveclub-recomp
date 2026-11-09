#pragma once
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <utility>
namespace Vulkan::SunAtlasProxy {
// CPU-only ownership handoff policy. No submit, work or completion callback runs
// while this mutex is held. Callers acquire this gate before any cache mutex.
class OwnerGate {
public:
    enum class Phase { Running, Closing, Closed };
    template<class IsOwner,class IsClosingOwner,class Submit,class Work>
    bool Run(IsOwner&& is_owner,IsClosingOwner&& is_closing_owner,Submit&& submit,Work&& work) {
        {
            std::unique_lock lock{mutex};
            if(phase==Phase::Closed) return false;
            if(phase==Phase::Closing) {
                // The current GPU owner must not wait for its own shutdown/drain.
                // This predicate uses captured IDs/thread-local state, not a dying owner pointer.
                if(is_closing_owner()) { lock.unlock();work();return true; }
                changed.wait(lock,[&]{return phase==Phase::Closed;});
                return false; // permanent renderer shutdown; no future cache consumers
            }
            ++active;
        }
        struct Lease {
            OwnerGate& gate;
            ~Lease() {
                std::lock_guard lock{gate.mutex};
                --gate.active;gate.changed.notify_all();
            }
        } lease{*this};
        // A live lease pins the dispatcher lifetime until submit+completion returns.
        if(is_owner()) work();else submit(std::forward<Work>(work));
        return true;
    }
    // Called before destroying the owner dispatcher. No cache lock may be held.
    void BeginClose() {
        std::unique_lock lock{mutex};
        if(phase!=Phase::Running) return;
        phase=Phase::Closing;changed.notify_all();
        changed.wait(lock,[&]{return active==0;});
    }
    // Called only after owner join, final recording-worker drain and GPU finish.
    void CompleteClose() {
        std::lock_guard lock{mutex};phase=Phase::Closed;changed.notify_all();
    }
    Phase GetPhase() { std::lock_guard lock{mutex};return phase; }
    std::size_t Active() { std::lock_guard lock{mutex};return active; }
private:
    std::mutex mutex;
    std::condition_variable changed;
    Phase phase=Phase::Running;
    std::size_t active=0;
};
}
