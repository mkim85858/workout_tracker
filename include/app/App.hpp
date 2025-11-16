#pragma once
#include "inference/PoseEstimator.hpp"
#include "control/StepperController.hpp"
#include "control/ServoController.hpp"
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <queue>

struct RepEvent {
    uint64_t ts_ms = 0;
    uint16_t exercise_id = 0;
    uint32_t delta = 1;   // reps added in this event
    uint32_t total = 0;   // filled by App when applied
};

class App {
public:
    App();
    ~App();

    // lifecycle
    void init();   // construct submodules later; validate config
    void start();  // spawn threads
    void stop();   // signal cancellation and join
    void wait();   // block until stop() (used by main)

    // This is what inference will call later.
    void onRepDetected(const RepEvent& e);

    // Simple health dump (stdout for now)
    void dumpStatus() const;

private:
    void loopThreadFunc();

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread loop_thread_;

    // very simple event queue for prototype
    mutable std::mutex q_mtx_;
    std::condition_variable q_cv_;
    std::queue<RepEvent> rep_queue_;

    std::unique_ptr<StepperController> stepper_;
    std::unique_ptr<ServoController> servo_;
    // pose estimator
    std::unique_ptr<PoseEstimator> pose_estimator_;

    // state
    std::atomic<uint32_t> rep_count_{0};
};
