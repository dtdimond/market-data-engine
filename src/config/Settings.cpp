#include "config/Settings.hpp"

#include <cstdlib>

namespace mde::config {

namespace {

std::string env_or(const char* name, const std::string& fallback) {
    const char* val = std::getenv(name);
    return val ? std::string(val) : fallback;
}

int env_int_or(const char* name, int fallback) {
    const char* val = std::getenv(name);
    if (!val) return fallback;
    try {
        return std::stoi(val);
    } catch (...) {
        return fallback;
    }
}

} // namespace

Settings Settings::from_environment() {
    std::string env = env_or("MDE_ENV", "development");
    Settings s = (env == "production") ? production() : development();
    s.websocket.url = env_or("MDE_WEBSOCKET_URL", s.websocket.url);
    s.websocket.ping_interval_seconds = env_int_or("MDE_PING_INTERVAL", s.websocket.ping_interval_seconds);
    s.api.gamma_api_base_url = env_or("MDE_GAMMA_API_URL", s.api.gamma_api_base_url);
    s.service.snapshot_interval_seconds = env_int_or("MDE_SNAPSHOT_INTERVAL", s.service.snapshot_interval_seconds);
    s.storage.backend = env_or("MDE_STORAGE_BACKEND", s.storage.backend);
    s.storage.data_directory = env_or("MDE_DATA_DIRECTORY", s.storage.data_directory);
    s.storage.write_buffer_size = env_int_or("MDE_WRITE_BUFFER_SIZE", s.storage.write_buffer_size);
    s.storage.s3_bucket = env_or("MDE_S3_BUCKET", s.storage.s3_bucket);
    s.storage.s3_prefix = env_or("MDE_S3_PREFIX", s.storage.s3_prefix);
    s.storage.s3_region = env_or("MDE_S3_REGION", s.storage.s3_region);
    s.storage.s3_endpoint_override = env_or("MDE_S3_ENDPOINT", s.storage.s3_endpoint_override);
    s.storage.s3_scheme = env_or("MDE_S3_SCHEME", s.storage.s3_scheme);
    return s;
}

Settings Settings::development() {
    Settings s;
    s.websocket.ping_interval_seconds = 30;
    s.service.snapshot_interval_seconds = 10;
    s.storage.data_directory = "data/dev";
    return s;
}

Settings Settings::production() {
    Settings s;
    s.websocket.ping_interval_seconds = 15;
    s.service.snapshot_interval_seconds = 5;
    s.storage.backend = "parquet";
    s.storage.data_directory = "data/prod";
    s.storage.write_buffer_size = 4096;
    return s;
}

} // namespace mde::config
