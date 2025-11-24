#pragma once
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

class App {
public:
    App();
    ~App();

    // lifecycle
    void init();   // construct submodules later; validate config
    void start();  // spawn threads
    void stop();   // signal cancellation and join
    void wait();   // block until stop() (used by main)

private:
    void loopThreadFunc();

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread loop_thread_;

    // very simple event queue for prototype
    mutable std::mutex q_mtx_;
    std::condition_variable q_cv_;

    std::unique_ptr<StepperController> stepper_;
    std::unique_ptr<ServoController> servo_;

    int port_ = 5005;
    int server_fd_ = -1;
    int client_fd_ = -1;

    void closeServer();
};