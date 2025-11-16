#include "app/App.hpp"
#include <iostream>
#include <vector>

App::App() = default;

App::~App() {
    stop();
}

void App::init() {
    pose_estimator_ = std::make_unique<PoseEstimator>(5005);
    std::vector<unsigned int> pins = {105, 106, 41, 43};
    motor_ = std::make_unique<MotorController>(pins);
}

void App::start() {
    if (running_.exchange(true)) return;
    stop_requested_ = false;

    pose_estimator_->start();

    motor_->start();

    // Launch a simple loop thread.
    loop_thread_ = std::thread(&App::loopThreadFunc, this);

    std::cout << "[app] Started.\n";
}

void App::stop() {
    if (!running_.exchange(false)) return;
    stop_requested_ = true;
    q_cv_.notify_all();
    if (loop_thread_.joinable()) loop_thread_.join();
    
    if (motor_) motor_->stop();
    if (pose_estimator_) pose_estimator_->stop();

    std::cout << "[app] Stopped.\n";
}

void App::wait() {
    // In a richer app this might wait on multiple modules; for now just join loop.
    if (loop_thread_.joinable()) loop_thread_.join();
}

void App::onRepDetected(const RepEvent& e) {
    // push into queue (non-blocking prototype)
    {
        std::lock_guard<std::mutex> lk(q_mtx_);
        rep_queue_.push(e);
    }
    q_cv_.notify_one();
}

void App::dumpStatus() const {
    std::cout << "[app] total_reps=" << rep_count_.load() << "\n";
}

void App::loopThreadFunc() {
    // Prototype event pump:
    // - If no events, print a heartbeat every ~2s
    // - If events, apply to rep_count and print an update
    auto last_heartbeat = std::chrono::steady_clock::now();
    while (!stop_requested_) {
        // Wait for event or timeout so we can also service pose commands.
        std::unique_lock<std::mutex> lk(q_mtx_);
        q_cv_.wait_for(lk, std::chrono::milliseconds(500),
                       [&]{ return stop_requested_ || !rep_queue_.empty(); });
        if (stop_requested_) break;

        // Pull out any pending rep events while holding the lock.
        std::vector<RepEvent> events;
        while (!rep_queue_.empty()) {
            events.push_back(rep_queue_.front());
            rep_queue_.pop();
        }
        lk.unlock();

        int processed = 0;

        for (auto& ev : events) {
            // Update totals and print a simple update.
            const auto new_total = rep_count_.fetch_add(ev.delta) + ev.delta;
            ++processed;
            std::cout << "[app] reps +" << ev.delta << " (exercise "
                      << ev.exercise_id << ") → total=" << new_total << "\n";
        }

        // Drain pose-estimator commands irrespective of events.
        int cmd = 0;
        while (pose_estimator_ && pose_estimator_->getNextCommand(cmd)) {
            ++processed;
            if (motor_) {
                motor_->pushCommand(cmd);
            } else {
                std::cerr << "[app] Motor not initialized; dropping cmd " << cmd << "\n";
            }
        }

        auto now = std::chrono::steady_clock::now();
        if (processed == 0 && now - last_heartbeat > std::chrono::seconds(2)) {
            std::cout << "[app] heartbeat… reps=" << rep_count_.load() << "\n";
            last_heartbeat = now;
        }
    }
}
