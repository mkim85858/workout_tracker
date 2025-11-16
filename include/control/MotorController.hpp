#pragma once
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <gpiod.h>

/**
 * MotorController controls a 4-wire motor through a ULN2003 driver
 * using the Jetson libgpiod character-device GPIO interface.
 *
 * cmd = 0 → rotate clockwise
 * cmd = 1 → rotate counter-clockwise
 */
class MotorController {
public:
    MotorController(const std::vector<unsigned int>& gpioLines,
                      const std::string& chipName = "gpiochip0");
    ~MotorController();

    void start();
    void stop();

    // Called by App (via inference) to queue new motion commands
    void pushCommand(int cmd);

private:
    void controlLoop();
    void stepCW(int steps);
    void stepCCW(int steps);
    void setupGPIO();
    void cleanupGPIO();

    std::string chipName_;
    std::vector<unsigned int> lines_;  // GPIO line offsets on the chip
    gpiod_chip* chip_ = nullptr;
    std::vector<gpiod_line*> handles_;

    std::thread controlThread_;
    std::atomic<bool> running_{false};

    std::queue<int> cmdQueue_;
    std::mutex qMutex_;
    std::condition_variable qCv_;
};
