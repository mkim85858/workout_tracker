#include "control/StepperController.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cmath>

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

        double delta = 0.0;
        if (cmd == 82) {
            delta = -commandStepDeg_;
        } else if (cmd == 76) {
            delta = commandStepDeg_;
        } else {
            std::cout << "[Stepper] Unknown cmd: " << cmd << "\n";
            continue;
        }

        desiredAngleDeg_ += delta;
        double target = normalizeAngle(desiredAngleDeg_);
        moveToAngle(target);
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
    {1,0,0,0},
    {1,1,0,0},
    {0,1,0,0},
    {0,1,1,0},
    {0,0,1,0},
    {0,0,1,1},
    {0,0,0,1},
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

void StepperController::moveToAngle(double targetDeg) {
    double diff = targetDeg - currentAngleDeg_;
    if (std::abs(diff) < 1e-3) {
        return;
    }
    int steps = static_cast<int>(std::round(std::abs(diff) * stepsPerDegree_));
    if (steps <= 0) return;

    if (diff > 0) {
        std::cout << "[Stepper] Move +" << diff << "° (" << steps << " steps)\n";
        stepCCW(steps);
    } else {
        std::cout << "[Stepper] Move " << diff << "° (" << steps << " steps)\n";
        stepCW(steps);
    }
    currentAngleDeg_ = targetDeg;
}

double StepperController::normalizeAngle(double requested) const {
    double result = requested;

    while (result < minAngleDeg_ - 360.0) result += 360.0;
    while (result > maxAngleDeg_ + 360.0) result -= 360.0;

    if (result < minAngleDeg_) {
        double wrapped = result + 360.0;
        if (wrapped <= maxAngleDeg_) {
            result = wrapped;
        } else {
            result = minAngleDeg_;
        }
    } else if (result > maxAngleDeg_) {
        double wrapped = result - 360.0;
        if (wrapped >= minAngleDeg_) {
            result = wrapped;
        } else {
            result = maxAngleDeg_;
        }
    }
    return std::clamp(result, minAngleDeg_, maxAngleDeg_);
}
