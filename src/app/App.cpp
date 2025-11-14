#include "app/App.hpp"
#include <iostream>
#include <iomanip>
#include <ctime>

static uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

App::App(const AppConfig& cfg) : config_(cfg) {}

App::~App() {
    stop();
}

void App::init() {
    // Minimal validation; BLE details will be checked when we wire BLE.
    if (config_.ble.adapter.empty()) {
        throw std::runtime_error("config.ble.adapter must not be empty");
    }
    // For prototype we just print what we parsed:
    std::cout << "[app] Config loaded. BLE adapter=" << config_.ble.adapter << "\n";
    ble_client_ = std::make_unique<BLEClient>(config_.ble);
    ble_client_->init();

    pose_estimator_ = std::make_unique<PoseEstimator>(5005);
    std::vector<unsigned int> pins = {105, 106, 41, 43};
    motor_ = std::make_unique<MotorController>(pins);
}

void App::start() {
    if (running_.exchange(true)) return;
    stop_requested_ = false;

    ble_client_->start();

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

    if (ble_client_) ble_client_->stop();
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
        // Wait for event or timeout
        std::unique_lock<std::mutex> lk(q_mtx_);
        q_cv_.wait_for(lk, std::chrono::milliseconds(500), [&]{ return stop_requested_ || !rep_queue_.empty(); });

        // BLE
        // Drain a few events quickly
        int processed = 0;
        /*
        while (!rep_queue_.empty()) {
            RepEvent ev = rep_queue_.front();
            rep_queue_.pop();
            lk.unlock();

            uint32_t new_total = rep_count_.fetch_add(ev.delta) + ev.delta;
            std::cout << "[rep] +" << ev.delta << " (exercise " << ev.exercise_id << ") total=" << new_total << "\n";
            WorkoutData w { 
                "bench_press", 
                45.0f, 
                new_total 
            };
            if (ble_client_) ble_client_->sendWorkoutUpdate(w);

            processed++;
            lk.lock();
        }
        lk.unlock();
        */

        // POSE ESTIMATOR
        int cmd;
        while (pose_estimator_ && pose_estimator_->getNextCommand(cmd)) {
            motor_->pushCommand(cmd);
        }


        auto now = std::chrono::steady_clock::now();
        if (processed == 0 && now - last_heartbeat > std::chrono::seconds(2)) {
            std::cout << "[app] heartbeat… reps=" << rep_count_.load() << "\n";
            last_heartbeat = now;
        }
    }
}
