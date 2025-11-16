#pragma once
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>
#include <gpiod.h>

/**
 * StepperController drives a 4-wire stepper via a ULN2003 driver using libgpiod.
 *
 * cmd = 0 → rotate clockwise
 * cmd = 1 → rotate counter-clockwise
 */
class StepperController {
public:
    StepperController(const std::vector<unsigned int>& gpioLines,
                      const std::string& chipName = "gpiochip0");
    ~StepperController();

    void start();
    void stop();

    void pushCommand(int cmd);

private:
    void controlLoop();
    void stepCW(int steps);
    void stepCCW(int steps);

    void setupGPIO();
    void cleanupGPIO();

    std::string chipName_;
    std::vector<unsigned int> lines_;
    gpiod_chip* chip_ = nullptr;
    std::vector<gpiod_line*> handles_;

    std::thread controlThread_;
    std::atomic<bool> running_{false};

    std::queue<int> cmdQueue_;
    std::mutex qMutex_;
    std::condition_variable qCv_;
};
