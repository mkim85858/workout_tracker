#include "app/App.hpp"
#include "app/Config.hpp"
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
    // Config path can be overridden by first arg or APP_CONFIG env.
    std::string cfg_path = "config/app.yaml";
    if (const char* envp = std::getenv("APP_CONFIG")) cfg_path = envp;
    if (argc > 1) cfg_path = argv[1];

    AppConfig cfg;
    try {
        cfg = load_config_from_file(cfg_path);
    } catch (const std::exception& e) {
        std::cerr << "[fatal] " << e.what() << "\n";
        return 1;
    }

    App app(cfg);
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

    /*
    // For now, simulate a few rep events so you can see output.
    for (int i = 0; i < 50; ++i) {
        RepEvent ev;
        ev.ts_ms = 0;          // not used yet
        ev.exercise_id = 1;
        ev.delta = 1;
        app.onRepDetected(ev);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    */

    // Run until Ctrl+C (or systemd stop)
    while (!g_sigint.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "[main] signal received; stopping…\n";
    app.stop();
    return 0;
}
