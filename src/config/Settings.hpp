#pragma once

#include <string>

namespace mde::config {

struct WebSocketSettings {
    std::string url = "wss://ws-subscriptions-clob.polymarket.com/ws/market";
    int ping_interval_seconds = 30;
};

struct ApiSettings {
    std::string gamma_api_base_url = "https://gamma-api.polymarket.com";
};

struct ServiceSettings {
    int snapshot_interval_seconds = 10;
};

struct StorageSettings {
    std::string data_directory = "data";
    int write_buffer_size = 1024;
};

struct Settings {
    WebSocketSettings websocket;
    ApiSettings api;
    ServiceSettings service;
    StorageSettings storage;

    static Settings from_environment();
    static Settings development();
    static Settings production();
};

} // namespace mde::config
