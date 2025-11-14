#pragma once
#include <string>
#include <stdexcept>

// Keep config structs simple & POD-like for now.
struct BleConfig {
    std::string adapter = "hci0";
    std::string target_address;      // preferred selector
    std::string target_name;         // alternative selector
    std::string service_uuid;        // used later
    std::string char_uuid_tx;        // used later
    bool write_without_response = true;

    int scan_timeout_ms = 5000;
    int connect_timeout_ms = 8000;
    int retry_backoff_ms = 1000;
    int burst_coalesce_ms = 250;
};

struct AppConfig {
    BleConfig ble;
};

// Throws std::runtime_error on parse errors.
AppConfig load_config_from_file(const std::string& path);
