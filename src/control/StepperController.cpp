#include "control/StepperController.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>

StepperController::StepperController(const std::vector<unsigned int>& gpioLines,
                                     const std::string& chipName)
    : chipName_(chipName), lines_(gpioLines) {}

StepperController::~StepperController() {
    stop();
}

void StepperController::start() {
    if (running_) return;
    setupGPIO();
    running_ = true;
    controlThread_ = std::thread(&StepperController::controlLoop, this);
    std::cout << "[Stepper] Started using " << chipName_ << "\n";
}

void StepperController::stop() {
    if (!running_) return;
    running_ = false;
    qCv_.notify_all();
    if (controlThread_.joinable()) controlThread_.join();
    cleanupGPIO();
    std::cout << "[Stepper] Stopped.\n";
}

void StepperController::pushCommand(int cmd) {
    {
        std::lock_guard<std::mutex> lk(qMutex_);
        while (!cmdQueue_.empty()) {
            cmdQueue_.pop();
        }
        cmdQueue_.push(cmd);
    }
    qCv_.notify_one();
}

void StepperController::controlLoop() {
    while (running_) {
        std::unique_lock<std::mutex> lk(qMutex_);
        qCv_.wait(lk, [&]{ return !cmdQueue_.empty() || !running_; });
        if (!running_) break;

        int cmd = cmdQueue_.front();
        cmdQueue_.pop();
        lk.unlock();

        if (cmd == 34) {
            std::cout << "[Stepper] Command: CW\n";
            stepCW(stepsPerCommand_);
        } else if (cmd == 28) {
            std::cout << "[Stepper] Command: CCW\n";
            stepCCW(stepsPerCommand_);
        } else {
            std::cout << "[Stepper] Unknown cmd: " << cmd << "\n";
        }
    }
}

/* ---------------- GPIO setup / teardown ---------------- */

void StepperController::setupGPIO() {
    chip_ = gpiod_chip_open_by_name(chipName_.c_str());
    if (!chip_) {
        throw std::runtime_error("[Stepper] Failed to open " + chipName_);
    }

    for (unsigned int lineOffset : lines_) {
        gpiod_line* line = gpiod_chip_get_line(chip_, lineOffset);
        if (!line) {
            throw std::runtime_error("[Stepper] Failed to get line " + std::to_string(lineOffset));
        }
        if (gpiod_line_request_output(line, "Stepper", 0) < 0) {
            throw std::runtime_error("[Stepper] Failed to request line " + std::to_string(lineOffset));
        }
        handles_.push_back(line);
    }

    std::cout << "[Stepper] GPIO lines configured\n";
}

void StepperController::cleanupGPIO() {
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

static const std::vector<std::vector<int>> HALFSTEP_SEQ = {
    //{1,0,0,0},
    {1,1,0,0},
    //{0,1,0,0},
    {0,1,1,0},
    //{0,0,1,0},
    {0,0,1,1},
    //{0,0,0,1},
    {1,0,0,1}
};

void StepperController::stepCW(int steps) {
    int n = HALFSTEP_SEQ.size();
    for (int i = 0; i < steps && running_; ++i) {
        const auto& pattern = HALFSTEP_SEQ[i % n];
        for (size_t j = 0; j < handles_.size(); ++j) {
            gpiod_line_set_value(handles_[j], pattern[j]);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }

    for (size_t j = 0; j < handles_.size(); ++j)
        gpiod_line_set_value(handles_[j], 0);
}

void StepperController::stepCCW(int steps) {
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
