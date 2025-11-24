#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

/**
 * Lightweight PWM-based servo controller using the sysfs PWM interface.
 *
 * Commands are integers shared with the inference stream:
 *   2 → move toward minimum angle
 *   3 → move toward maximum angle
 * Additional integers are ignored for now.
 */
class ServoController {
public:
    ServoController(const std::string& chipPath = "/sys/class/pwm/pwmchip0",
                    unsigned int channel = 0);
    ~ServoController();

    void start();
    void stop();

    void pushCommand(int cmd);

private:
    void controlLoop();
    void setupPWM();
    void teardownPWM();
    void writeFile(const std::string& path, const std::string& value);

    const std::string chipPath_;
    const unsigned int channel_;
    std::string pwmPath_;

    const uint32_t periodNs_ = 20000000;      // 20 ms (50 Hz)
    const uint32_t minDutyNs_ = 500000;      // 0.5 ms pulse
    const uint32_t maxDutyNs_ = 2500000;      // 2.5 ms pulse
    uint32_t dutyNs_          = 2500000;      // start lowered

    std::thread worker_;
    std::atomic<bool> running_{false};

    std::mutex qMutex_;
    std::condition_variable qCv_;
    std::queue<int> queue_;
};
