#pragma once
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>

class PoseEstimator {
public:
    PoseEstimator(int port = 5005);
    ~PoseEstimator();

    void start();
    void stop();

    bool getNextCommand(int &cmd);  // 0 or 1

private:
    void serverLoop();

    int server_fd;
    int client_fd;
    int port;

    std::thread serverThread;
    std::atomic<bool> running;

    std::queue<int> cmdQueue;
    std::mutex queueMutex;
};
