#include "app/App.hpp"
#include <csignal>
#include <atomic>
#include <iostream>
#include <thread>
#include <chrono>

static std::atomic<bool> g_sigint{false};

static void handle_signal(int) {
    g_sigint.store(true);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    App app;
    try {
        app.init();
    } catch (const std::exception& e) {
        std::cerr << "[fatal] init failed: " << e.what() << "\n";
        return 2;
    }

    // Signals
    std::signal(SIGINT,  handle_signal);
    std::signal(SIGTERM, handle_signal);

    app.start();

    // Run until Ctrl+C (or systemd stop)
    while (!g_sigint.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "[main] signal received; stopping…\n";
    app.stop();
    return 0;
}
