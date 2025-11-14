#include "control/MotorController.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>

MotorController::MotorController(const std::vector<unsigned int>& gpioLines,
                                     const std::string& chipName)
    : chipName_(chipName), lines_(gpioLines) {}

MotorController::~MotorController() {
    stop();
}

void MotorController::start() {
    if (running_) return;
    setupGPIO();
    running_ = true;
    controlThread_ = std::thread(&MotorController::controlLoop, this);
    std::cout << "[Motor] Started using " << chipName_ << "\n";
}

void MotorController::stop() {
    if (!running_) return;
    running_ = false;
    qCv_.notify_all();
    if (controlThread_.joinable()) controlThread_.join();
    cleanupGPIO();
    std::cout << "[Motor] Stopped.\n";
}

void MotorController::pushCommand(int cmd) {
    {
        std::lock_guard<std::mutex> lk(qMutex_);
        cmdQueue_.push(cmd);
    }
    qCv_.notify_one();
}

void MotorController::controlLoop() {
    while (running_) {
        std::unique_lock<std::mutex> lk(qMutex_);
        qCv_.wait(lk, [&]{ return !cmdQueue_.empty() || !running_; });
        if (!running_) break;

        int cmd = cmdQueue_.front();
        cmdQueue_.pop();
        lk.unlock();

        if (cmd == 0) {
            std::cout << "[Motor] Rotate CW\n";
            stepCW(512); // one revolution (512 half-steps typical for 28BYJ-48)
        } else if (cmd == 1) {
            std::cout << "[Motor] Rotate CCW\n";
            stepCCW(512);
        } else {
            std::cout << "[Motor] Unknown cmd: " << cmd << "\n";
        }
    }
}

/* ---------------- GPIO setup / teardown ---------------- */

void MotorController::setupGPIO() {
    chip_ = gpiod_chip_open_by_name(chipName_.c_str());
    if (!chip_) {
        throw std::runtime_error("[Motor] Failed to open " + chipName_);
    }

    for (unsigned int lineOffset : lines_) {
        gpiod_line* line = gpiod_chip_get_line(chip_, lineOffset);
        if (!line) {
            throw std::runtime_error("[Motor] Failed to get line " + std::to_string(lineOffset));
        }
        if (gpiod_line_request_output(line, "Motor", 0) < 0) {
            throw std::runtime_error("[Motor] Failed to request line " + std::to_string(lineOffset));
        }
        handles_.push_back(line);
    }

    std::cout << "[Motor] GPIO lines configured\n";
}

void MotorController::cleanupGPIO() {
    for (auto* line : handles_) {
        if (line) gpiod_line_release(line);
    }
    handles_.clear();

    if (chip_) {
        gpiod_chip_close(chip_);
        chip_ = nullptr;
    }
}

/* ---------------- Step logic ---------------- */

// 8 half-step sequence for ULN2003 driver
static const std::vector<std::vector<int>> HALFSTEP_SEQ = {
    {1,0,0,0},
    {1,1,0,0},
    {0,1,0,0},
    {0,1,1,0},
    {0,0,1,0},
    {0,0,1,1},
    {0,0,0,1},
    {1,0,0,1}
};

void MotorController::stepCW(int steps) {
    int n = HALFSTEP_SEQ.size();
    for (int i = 0; i < steps && running_; ++i) {
        const auto& pattern = HALFSTEP_SEQ[i % n];
        for (size_t j = 0; j < handles_.size(); ++j) {
            gpiod_line_set_value(handles_[j], pattern[j]);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }

    // Turn off coils
    for (size_t j = 0; j < handles_.size(); ++j)
        gpiod_line_set_value(handles_[j], 0);
}

void MotorController::stepCCW(int steps) {
    std::vector<std::vector<int>> rev = HALFSTEP_SEQ;
    std::reverse(rev.begin(), rev.end());
    int n = rev.size();
    for (int i = 0; i < steps && running_; ++i) {
        const auto& pattern = rev[i % n];
        for (size_t j = 0; j < handles_.size(); ++j) {
            gpiod_line_set_value(handles_[j], pattern[j]);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }

    for (size_t j = 0; j < handles_.size(); ++j)
        gpiod_line_set_value(handles_[j], 0);
}