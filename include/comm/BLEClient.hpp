#pragma once
#include "app/Config.hpp"
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>

// Small struct describing what we send.
struct WorkoutData {
    std::string type;
    float weight;
    uint32_t reps;
};

namespace sdbus { class IConnection; } // fwd decl

class BLEClient {
public:
    explicit BLEClient(const BleConfig& cfg);
    ~BLEClient();

    void init();
    void start();
    void stop();

    // Non-blocking enqueue for outbound data.
    void sendWorkoutUpdate(const WorkoutData& data);

private:
    void threadFunc();

    // --- BlueZ helpers ---
    std::string devicePathFromMac(const std::string& mac) const;
    // returns empty string on failure
    std::string findServicePathByUuid(sdbus::IConnection& conn,
                                      const std::string& devicePath,
                                      const std::string& serviceUuid);
    std::string findCharPathByUuid(sdbus::IConnection& conn,
                                   const std::string& devicePath,
                                   const std::string& servicePath,
                                   const std::string& charUuid);

    // --- Write ---
    bool writeJsonToChar(sdbus::IConnection& conn,
                         const std::string& charPath,
                         const std::string& json);

    BleConfig config_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    std::mutex q_mtx_;
    std::condition_variable q_cv_;
    std::queue<WorkoutData> queue_;

    bool connected_ = false;

    // cached discovered paths
    std::string cached_device_path_;
    std::string cached_service_path_;
    std::string cached_char_path_;
};
