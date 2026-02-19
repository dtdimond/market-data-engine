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

struct DiscoverySettings {
    bool enabled = false;
    int max_tracked_markets = 500;
    int discovery_interval_seconds = 1800;  // 30 min
    int markets_per_poll = 50;
};

struct StorageSettings {
    std::string backend = "memory";       // "memory", "parquet", or "s3"
    std::string data_directory = "data";
    int write_buffer_size = 1024;
    // S3-compatible storage (AWS S3, Cloudflare R2, Backblaze B2, Wasabi, MinIO)
    std::string s3_bucket;
    std::string s3_prefix = "mde";
    std::string s3_region = "us-east-1";
    std::string s3_endpoint_override;     // non-empty for R2/B2/Wasabi/MinIO
    std::string s3_scheme = "https";      // "http" for local MinIO
};

struct Settings {
    WebSocketSettings websocket;
    ApiSettings api;
    ServiceSettings service;
    DiscoverySettings discovery;
    StorageSettings storage;

    static Settings from_environment();
    static Settings development();
    static Settings production();
};

} // namespace mde::config
