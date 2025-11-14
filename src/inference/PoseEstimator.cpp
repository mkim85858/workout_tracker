#include "inference/PoseEstimator.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

PoseEstimator::PoseEstimator(int port_) 
    : server_fd(-1), client_fd(-1), port(port_), running(false) {}

PoseEstimator::~PoseEstimator() {
    stop();
}

void PoseEstimator::start() {
    running = true;
    serverThread = std::thread(&PoseEstimator::serverLoop, this);
}

void PoseEstimator::stop() {
    running = false;
    if (server_fd != -1) close(server_fd);
    if (client_fd != -1) close(client_fd);
    if (serverThread.joinable()) serverThread.join();
}

bool PoseEstimator::getNextCommand(int &cmd) {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (cmdQueue.empty()) return false;
    cmd = cmdQueue.front();
    cmdQueue.pop();
    return true;
}

void PoseEstimator::serverLoop() {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;   // 0.0.0.0
    addr.sin_port = htons(port);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "[PoseEstimator] Socket creation failed\n";
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[PoseEstimator] Bind failed\n";
        return;
    }

    if (listen(server_fd, 1) < 0) {
        std::cerr << "[PoseEstimator] Listen failed\n";
        return;
    }

    std::cout << "[PoseEstimator] Listening on port " << port << "...\n";

    while (running) {
        sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);

        client_fd = accept(server_fd, (struct sockaddr*)&clientAddr, &addrLen);
        if (client_fd < 0) {
            if (!running) break;
            std::cerr << "[PoseEstimator] Accept failed\n";
            continue;
        }

        std::cout << "[PoseEstimator] Client connected\n";

        char buffer[2];
        while (running) {
            ssize_t bytes = recv(client_fd, buffer, 1, 0);
            if (bytes <= 0) {
                std::cout << "[PoseEstimator] Client disconnected\n";
                close(client_fd);
                client_fd = -1;
                break;
            }

            int cmd = buffer[0] - '0';  // Expect '0' or '1'
            if (cmd == 0 || cmd == 1) {
                std::lock_guard<std::mutex> lock(queueMutex);
                cmdQueue.push(cmd);
                std::cout << "[PoseEstimator] Received cmd: " << cmd << "\n";
            }
        }
    }
}
