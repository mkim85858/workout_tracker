#include "app/Config.hpp"
#include <yaml-cpp/yaml.h>

static BleConfig parse_ble(const YAML::Node& node) {
    BleConfig cfg;

    if (!node || !node.IsMap()) return cfg; // leave defaults

    if (auto v = node["adapter"])                 cfg.adapter = v.as<std::string>(cfg.adapter);
    if (auto v = node["target_address"])         cfg.target_address = v.as<std::string>("");
    if (auto v = node["target_name"])            cfg.target_name = v.as<std::string>("");
    if (auto v = node["service_uuid"])           cfg.service_uuid = v.as<std::string>("");
    if (auto v = node["char_uuid_tx"])           cfg.char_uuid_tx = v.as<std::string>("");
    if (auto v = node["write_without_response"]) cfg.write_without_response = v.as<bool>(cfg.write_without_response);

    if (auto v = node["scan_timeout_ms"])        cfg.scan_timeout_ms = v.as<int>(cfg.scan_timeout_ms);
    if (auto v = node["connect_timeout_ms"])     cfg.connect_timeout_ms = v.as<int>(cfg.connect_timeout_ms);
    if (auto v = node["retry_backoff_ms"])       cfg.retry_backoff_ms = v.as<int>(cfg.retry_backoff_ms);
    if (auto v = node["burst_coalesce_ms"])      cfg.burst_coalesce_ms = v.as<int>(cfg.burst_coalesce_ms);

    return cfg;
}

AppConfig load_config_from_file(const std::string& path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(path);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to load YAML: ") + e.what());
    }

    AppConfig cfg;
    cfg.ble = parse_ble(root["ble"]);
    return cfg;
}
