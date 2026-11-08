#include "sun_atlas_proxy_owner_gate.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
using namespace std::chrono_literals;
using Vulkan::SunAtlasProxy::OwnerGate;
// A true single owner drains simulated recording work before processing
// callbacks, matching Liverpool::ProcessCommands. A guest never writes the
// owner queue directly.
int main() {
    OwnerGate gate;
    std::mutex queue_mutex, tc_mutex;
    std::condition_variable queue_cv;
    std::queue<std::function<void()>> requests;
    bool stop = false;
    std::thread::id owner_id;
    std::atomic<bool> tc_was_free = false, owner_ran = false, late_ran = false, inline_ran = false;
    std::promise<void> owner_ready, release_first, entered_first;
    auto released = release_first.get_future().share();
    std::jthread owner([&] {
        owner_id = std::this_thread::get_id();
        owner_ready.set_value();
        while (true) {
            std::function<void()> f;
            {
                std::unique_lock lock{queue_mutex};
                queue_cv.wait(lock, [&] { return stop || !requests.empty(); });
                if (stop && requests.empty())
                    return;
                f = std::move(requests.front());
                requests.pop();
            }
            f();
        }
    });
    owner_ready.get_future().wait();
    auto submit = [&](auto work) {
        std::promise<void> done;
        auto future = done.get_future();
        {
            std::lock_guard lock{queue_mutex};
            requests.emplace([work = std::move(work), &done]() mutable {
                work();
                done.set_value();
            });
        }
        queue_cv.notify_one();
        future.get();
    };
    auto is_owner = [&] { return std::this_thread::get_id() == owner_id; };
    std::jthread guest([&] {
        gate.Run(is_owner, is_owner, submit, [&] {
            assert(is_owner());
            tc_was_free = tc_mutex.try_lock();
            if (tc_was_free)
                tc_mutex.unlock();
            entered_first.set_value();
            released.wait();
            owner_ran = true;
        });
    });
    entered_first.get_future().wait();
    assert(gate.Active() == 1);
    std::promise<void> closing_ready, final_drain;
    auto drained = final_drain.get_future().share();
    std::jthread shutdown([&] {
        gate.BeginClose(); // waits for the first live dispatcher lease
        assert(owner_ran);
        closing_ready.set_value();
        submit([&] {
            assert(is_owner());
            // Owner work arriving during Closing executes inline, avoiding a
            // self-wait.
            auto no_submit = [](auto) { assert(false); };
            gate.Run(is_owner, is_owner, no_submit, [&] { inline_ran = true; });
        });
        drained.wait(); // models owner join + recording-worker drain + GPU finish
        gate.CompleteClose();
    });
    while (gate.GetPhase() != OwnerGate::Phase::Closing)
        std::this_thread::yield();
    auto late = std::async(std::launch::async, [&] {
        bool ran = gate.Run(is_owner, is_owner, submit, [&] { late_ran = true; });
        assert(!ran);
    });
    assert(late.wait_for(20ms) == std::future_status::timeout);
    release_first.set_value();
    guest.join();
    closing_ready.get_future().wait();
    assert(tc_was_free && owner_ran);
    assert(late.wait_for(20ms) == std::future_status::timeout);
    final_drain.set_value();
    shutdown.join();
    late.get();
    assert(inline_ran && !late_ran);
    bool touched = false;
    assert(!gate.Run(
        [&] {
            touched = true;
            return false;
        },
        [&] {
            touched = true;
            return false;
        },
        submit, [&] { touched = true; }));
    assert(!touched); // closed gate never even dereferences a destroyed dispatcher
    {
        std::lock_guard lock{queue_mutex};
        stop = true;
    }
    queue_cv.notify_one();
    owner.join();
    std::puts("PASS: guest handoff executes on owner without TC lock; Closing "
              "waits live lease; owner-inline avoids self-deadlock; late guests "
              "block until final drain; Closed never reads dispatcher");
}
