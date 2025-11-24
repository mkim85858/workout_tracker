#include "app/App.hpp"
#include <iostream>
#include <vector>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

App::App() = default;

App::~App() {
    stop();
}

void App::init() {
    std::vector<unsigned int> pins = {105, 106, 41, 43};
    stepper_ = std::make_unique<StepperController>(pins);
    servo_ = std::make_unique<ServoController>("/sys/class/pwm/pwmchip0", 0);
}

void App::start() {
    if (running_.exchange(true)) return;
    stop_requested_ = false;

    if (stepper_) stepper_->start();
    if (servo_) servo_->start();

    // Launch a simple loop thread.
    loop_thread_ = std::thread(&App::loopThreadFunc, this);

    std::cout << "[app] Started.\n";
}

void App::stop() {
    if (!running_.exchange(false)) return;
    stop_requested_ = true;
    closeServer();
    if (loop_thread_.joinable()) loop_thread_.join();
    
    if (servo_) servo_->stop();
    if (stepper_) stepper_->stop();

    std::cout << "[app] Stopped.\n";
}

void App::wait() {
    // In a richer app this might wait on multiple modules; for now just join loop.
    if (loop_thread_.joinable()) loop_thread_.join();
}

void App::loopThreadFunc() {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[app] Socket creation failed\n";
        return;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[app] Bind failed\n";
        closeServer();
        return;
    }

    if (listen(server_fd_, 1) < 0) {
        std::cerr << "[app] Listen failed\n";
        closeServer();
        return;
    }

    std::cout << "[app] Listening on port " << port_ << "...\n";

    while (!stop_requested_) {
        sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);
        client_fd_ = accept(server_fd_, (struct sockaddr*)&clientAddr, &addrLen);
        if (client_fd_ < 0) {
            if (stop_requested_) break;
            std::cerr << "[app] Accept failed\n";
            continue;
        }

        std::cout << "[app] Client connected\n";

        char buffer[2];
        while (!stop_requested_) {
            ssize_t bytes = recv(client_fd_, buffer, 1, 0);
            if (bytes <= 0) {
                std::cout << "[app] Client disconnected\n";
                close(client_fd_);
                client_fd_ = -1;
                break;
            }

            int cmd = buffer[0] - '0';
            switch (cmd) {
                case 34:
                case 28:
                    if (stepper_) {
                        stepper_->pushCommand(cmd);
                    } else {
                        std::cerr << "[app] Stepper not ready; drop cmd " << cmd << "\n";
                    }
                    break;
                case 2:
                case 3:
                case 4:
                    if (servo_) {
                        servo_->pushCommand(cmd);
                    } else {
                        std::cerr << "[app] Servo not ready; drop cmd " << cmd << "\n";
                    }
                    break;
                default:
                    std::cerr << "[app] Unknown pose cmd " << cmd << "\n";
                    break;
            }
        }
    }

    closeServer();
}

void App::closeServer() {
    if (client_fd_ != -1) {
        close(client_fd_);
        client_fd_ = -1;
    }
    if (server_fd_ != -1) {
        close(server_fd_);
        server_fd_ = -1;
    }
}
