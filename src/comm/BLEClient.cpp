#include "comm/BLEClient.hpp"
#include <sdbus-c++/sdbus-c++.h>
#include <chrono>
#include <thread>
#include <sstream>
#include <map>
#include <vector>

using namespace std::chrono_literals;

BLEClient::BLEClient(const BleConfig& cfg) : config_(cfg) {}
BLEClient::~BLEClient() { stop(); }

void BLEClient::init() {
    std::cout << "[ble] init adapter=" << config_.adapter
              << " target=" << (config_.target_address.empty()
                                 ? config_.target_name
                                 : config_.target_address)
              << "\n";
}

void BLEClient::start() {
    if (running_.exchange(true)) return;
    stop_requested_ = false;
    thread_ = std::thread(&BLEClient::threadFunc, this);
    std::cout << "[ble] started\n";
}

void BLEClient::stop() {
    if (!running_.exchange(false)) return;
    stop_requested_ = true;
    q_cv_.notify_all();
    if (thread_.joinable()) thread_.join();
    std::cout << "[ble] stopped\n";
}

void BLEClient::sendWorkoutUpdate(const WorkoutData& data) {
    {
        std::lock_guard<std::mutex> lk(q_mtx_);
        queue_.push(data);
    }
    q_cv_.notify_one();
}

std::string BLEClient::devicePathFromMac(const std::string& mac) const {
    std::string p;
    p.reserve(mac.size());
    for (char c : mac) p.push_back(c == ':' ? '_' : c);
    return "/org/bluez/" + config_.adapter + "/dev_" + p;
}

// Explore BlueZ object tree once and find the service path whose UUID matches.
std::string BLEClient::findServicePathByUuid(sdbus::IConnection& conn,
                                             const std::string& devicePath,
                                             const std::string& serviceUuid)
{
    auto om = sdbus::createProxy(conn, sdbus::ServiceName("org.bluez"),  sdbus::ObjectPath("/"));
    // GetManagedObjects returns: map<objectPath, map<interface, map<property, variant>>>
    std::map<sdbus::ObjectPath,
             std::map<std::string, std::map<std::string, sdbus::Variant>>> objs;

    om->callMethod("GetManagedObjects")
      .onInterface("org.freedesktop.DBus.ObjectManager")
      .storeResultsTo(objs);

    const std::string want = serviceUuid; // already lower/upper accepted by BlueZ
    for (const auto& [objPath, ifaces] : objs) {
        const std::string pathStr = static_cast<std::string>(objPath);
        // Restrict search to under this device
        if (pathStr.rfind(devicePath + "/", 0) != 0) continue;

        auto it = ifaces.find("org.bluez.GattService1");
        if (it == ifaces.end()) continue;

        const auto& props = it->second;
        auto uuidIt = props.find("UUID");
        if (uuidIt == props.end()) continue;

        const auto& variant = uuidIt->second;
        const std::string uuid = variant.get<std::string>();
        if (!uuid.empty() && strcasecmp(uuid.c_str(), want.c_str()) == 0) {
            return pathStr;
        }
    }
    return {};
}

std::string BLEClient::findCharPathByUuid(sdbus::IConnection& conn,
                                          const std::string& devicePath,
                                          const std::string& servicePath,
                                          const std::string& charUuid)
{
    auto om = sdbus::createProxy(conn, sdbus::ServiceName("org.bluez"), sdbus::ObjectPath("/"));
    std::map<sdbus::ObjectPath,
             std::map<std::string, std::map<std::string, sdbus::Variant>>> objs;

    om->callMethod("GetManagedObjects")
      .onInterface("org.freedesktop.DBus.ObjectManager")
      .storeResultsTo(objs);

    const std::string want = charUuid;
    for (const auto& [objPath, ifaces] : objs) {
        const std::string pathStr = static_cast<std::string>(objPath);
        // Must be under devicePath and belong to this service subtree
        if (pathStr.rfind(devicePath + "/", 0) != 0) continue;
        if (pathStr.rfind(servicePath + "/", 0) != 0) continue;

        auto it = ifaces.find("org.bluez.GattCharacteristic1");
        if (it == ifaces.end()) continue;

        const auto& props = it->second;
        auto uuidIt = props.find("UUID");
        if (uuidIt == props.end()) continue;

        const auto& variant = uuidIt->second;
        const std::string uuid = variant.get<std::string>();
        if (!uuid.empty() && strcasecmp(uuid.c_str(), want.c_str()) == 0) {
            return pathStr;
        }
    }
    return {};
}

bool BLEClient::writeJsonToChar(sdbus::IConnection& conn,
                                const std::string& charPath,
                                const std::string& json)
{
    auto charProxy = sdbus::createProxy(conn, sdbus::ServiceName("org.bluez"), sdbus::ObjectPath(charPath));

    // Build byte vector
    std::vector<uint8_t> bytes(json.begin(), json.end());

    // BlueZ WriteValue signature: aay + dict options
    std::map<std::string, sdbus::Variant> options;
    // If you want Write Without Response explicitly, you can hint via "type"
    // but BlueZ generally selects based on characteristic properties.
    // options["type"] = std::string("request");

    try {
        charProxy->callMethod("WriteValue")
            .onInterface("org.bluez.GattCharacteristic1")
            .withArguments(bytes, options);
        return true;
    } catch (const sdbus::Error& e) {
        std::cerr << "[ble] WriteValue error: " << e.getName()
                  << " " << e.getMessage() << "\n";
        return false;
    }
}

void BLEClient::threadFunc() {
    /*
    auto connection = sdbus::createSystemBusConnection();

    while (!stop_requested_) {
        try {
            // Resolve device path
            if (cached_device_path_.empty()) {
                if (config_.target_address.empty()) {
                    std::cerr << "[ble] target_address is required for now\n";
                    goto retry_sleep;
                }
                cached_device_path_ = devicePathFromMac(config_.target_address);
            }

            // Connect
            {
                auto dev = sdbus::createProxy(*connection, sdbus::ServiceName("org.bluez"), sdbus::ObjectPath(cached_device_path_));
                std::cout << "[ble] Connecting to " << cached_device_path_ << "...\n";
                try {
                    dev->callMethod("Connect").onInterface("org.bluez.Device1");
                } catch (const sdbus::Error& e) {
                    // If already connected, BlueZ returns an error; ignore benign ones.
                    std::cerr << "[ble] Connect: " << e.getName() << " " << e.getMessage() << "\n";
                }
                connected_ = true;
                std::cout << "[ble] connected\n";
            }

            // Wait until GATT services are resolved
            auto devProps = sdbus::createProxy(*connection, sdbus::ServiceName("org.bluez"), sdbus::ObjectPath(cached_device_path_));
            bool resolved = false;
            for (int i = 0; i < 20 && !resolved && !stop_requested_; ++i) {
                try {
                    devProps->callMethod("Get")
                        .onInterface("org.freedesktop.DBus.Properties")
                        .withArguments("org.bluez.Device1", "ServicesResolved")
                        .storeResultsTo(resolved);
                } catch (...) {
                    resolved = false;
                }
                if (!resolved) std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
            if (!resolved) {
                std::cerr << "[ble] warning: ServicesResolved=false after timeout\n";
            }


            // Discover service path by UUID
            if (cached_service_path_.empty()) {
                if (config_.service_uuid.empty()) {
                    std::cerr << "[ble] service_uuid missing in config\n";
                    goto disconnect_and_retry;
                }
                cached_service_path_ = findServicePathByUuid(*connection,
                                                             cached_device_path_,
                                                             config_.service_uuid);
                if (cached_service_path_.empty()) {
                    std::cerr << "[ble] could not find service by UUID\n";
                    goto disconnect_and_retry;
                }
                std::cout << "[ble] service path = " << cached_service_path_ << "\n";
            }

            // Discover characteristic path by UUID
            if (cached_char_path_.empty()) {
                if (config_.char_uuid_tx.empty()) {
                    std::cerr << "[ble] char_uuid_tx missing in config\n";
                    goto disconnect_and_retry;
                }
                cached_char_path_ = findCharPathByUuid(*connection,
                                                       cached_device_path_,
                                                       cached_service_path_,
                                                       config_.char_uuid_tx);
                if (cached_char_path_.empty()) {
                    std::cerr << "[ble] could not find characteristic by UUID\n";
                    goto disconnect_and_retry;
                }
                std::cout << "[ble] char path = " << cached_char_path_ << "\n";
            }

            // Drain queue and write
            while (!stop_requested_) {
                std::unique_lock<std::mutex> lk(q_mtx_);
                q_cv_.wait_for(lk, 500ms, [&]{ return stop_requested_ || !queue_.empty(); });

                while (!queue_.empty()) {
                    WorkoutData w = queue_.front();
                    queue_.pop();
                    lk.unlock();

                    std::ostringstream ss;
                    ss << "{\"type\":\"" << w.type
                       << "\",\"weight\":" << w.weight
                       << ",\"reps\":" << w.reps << "}";
                    const std::string payload = ss.str();

                    if (writeJsonToChar(*connection, cached_char_path_, payload)) {
                        std::cout << "[ble] write OK → " << payload << "\n";
                    } else {
                        std::cerr << "[ble] write failed, will reconnect\n";
                        goto disconnect_and_retry;
                    }

                    lk.lock();
                }
                lk.unlock();
            }

        disconnect_and_retry:
            if (connected_) {
                try {
                    auto dev = sdbus::createProxy(*connection, sdbus::ServiceName("org.bluez"),  sdbus::ObjectPath(cached_device_path_));
                    dev->callMethod("Disconnect").onInterface("org.bluez.Device1");
                    std::cout << "[ble] disconnected\n";
                } catch (...) {
                    // ignore
                }
                connected_ = false;
            }

        } catch (const sdbus::Error& e) {
            std::cerr << "[ble] error: " << e.getName() << " " << e.getMessage() << "\n";
            connected_ = false;
        }

    retry_sleep:
        if (!stop_requested_) {
            std::this_thread::sleep_for(2s);
            std::cout << "[ble] retrying…\n";
        }
    }
    */
}
