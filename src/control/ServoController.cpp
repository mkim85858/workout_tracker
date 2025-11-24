#include "control/ServoController.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

ServoController::ServoController(const std::string& chipPath, unsigned int channel)
    : chipPath_(chipPath), channel_(channel) {
    pwmPath_ = chipPath_ + "/pwm" + std::to_string(channel_);
}

ServoController::~ServoController() {
    stop();
}

void ServoController::start() {
    if (running_) return;
    setupPWM();
    running_ = true;
    worker_ = std::thread(&ServoController::controlLoop, this);
    std::cout << "[Servo] Started on " << pwmPath_ << "\n";
}

void ServoController::stop() {
    if (!running_) {
        teardownPWM();
        return;
    }
    running_ = false;
    qCv_.notify_all();
    if (worker_.joinable()) worker_.join();
    teardownPWM();
    std::cout << "[Servo] Stopped\n";
}

void ServoController::pushCommand(int cmd) {
    {
        std::lock_guard<std::mutex> lk(qMutex_);
        queue_.push(cmd);
    }
    qCv_.notify_one();
}

void ServoController::controlLoop() {
    while (running_) {
        std::unique_lock<std::mutex> lk(qMutex_);
        qCv_.wait(lk, [&] { return !queue_.empty() || !running_; });
        if (!running_) break;
        int cmd = queue_.front();
        queue_.pop();
        lk.unlock();

        if (cmd == 2) {  // lowest angle
            dutyNs_ = minDutyNs_ + (180.0f / 180.0f) * (maxDutyNs_ - minDutyNs_);
        } else if (cmd == 3) {  // middle angle
            dutyNs_ = minDutyNs_ + (162.5f / 180.0f) * (maxDutyNs_ - minDutyNs_);
        } else if (cmd == 4) {  // highest angle
            dutyNs_ = minDutyNs_ + (145.0f / 180.0f) * (maxDutyNs_ - minDutyNs_);
        } else {
            continue;  // ignore unrelated commands
        }

        try {
            writeFile(pwmPath_ + "/duty_cycle", std::to_string(dutyNs_));
        } catch (const std::exception& e) {
            std::cerr << "[Servo] Failed to update duty cycle: " << e.what() << "\n";
        }
    }
}

void ServoController::setupPWM() {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(pwmPath_, ec)) {
        writeFile(chipPath_ + "/export", std::to_string(channel_));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    writeFile(pwmPath_ + "/period", std::to_string(periodNs_));
    writeFile(pwmPath_ + "/duty_cycle", std::to_string(dutyNs_));
    writeFile(pwmPath_ + "/enable", "1");
}

void ServoController::teardownPWM() {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::exists(pwmPath_ + "/enable", ec)) {
        try {
            writeFile(pwmPath_ + "/enable", "0");
        } catch (...) {
            // ignore
        }
    }
    if (fs::exists(chipPath_ + "/unexport", ec)) {
        try {
            writeFile(chipPath_ + "/unexport", std::to_string(channel_));
        } catch (...) {
        }
    }
}

void ServoController::writeFile(const std::string& path, const std::string& value) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("[Servo] Failed to open " + path);
    }
    file << value;
    if (!file) {
        throw std::runtime_error("[Servo] Failed to write " + path);
    }
}