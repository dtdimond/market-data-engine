#include "config/Settings.hpp"

#include <gtest/gtest.h>

#include <cstdlib>

using namespace mde::config;

TEST(Settings, DefaultsAreReasonable) {
    Settings s;
    EXPECT_EQ(s.websocket.url, "wss://ws-subscriptions-clob.polymarket.com/ws/market");
    EXPECT_EQ(s.websocket.ping_interval_seconds, 30);
    EXPECT_EQ(s.api.gamma_api_base_url, "https://gamma-api.polymarket.com");
    EXPECT_EQ(s.service.snapshot_interval_seconds, 10);
    EXPECT_EQ(s.storage.backend, "memory");
    EXPECT_EQ(s.storage.data_directory, "data");
    EXPECT_EQ(s.storage.write_buffer_size, 1024);
    EXPECT_FALSE(s.discovery.enabled);
    EXPECT_EQ(s.discovery.max_tracked_markets, 500);
    EXPECT_EQ(s.discovery.discovery_interval_seconds, 1800);
    EXPECT_EQ(s.discovery.markets_per_poll, 50);
}

TEST(Settings, FromEnvironmentDefaultsToDevelopment) {
    // Unset any MDE_ vars that might be set
    unsetenv("MDE_ENV");
    unsetenv("MDE_WEBSOCKET_URL");
    unsetenv("MDE_PING_INTERVAL");
    unsetenv("MDE_GAMMA_API_URL");
    unsetenv("MDE_SNAPSHOT_INTERVAL");
    unsetenv("MDE_STORAGE_BACKEND");
    unsetenv("MDE_DATA_DIRECTORY");
    unsetenv("MDE_WRITE_BUFFER_SIZE");
    unsetenv("MDE_DISCOVERY_ENABLED");
    unsetenv("MDE_MAX_TRACKED_MARKETS");
    unsetenv("MDE_DISCOVERY_INTERVAL");
    unsetenv("MDE_MARKETS_PER_POLL");

    auto s = Settings::from_environment();
    auto dev = Settings::development();
    EXPECT_EQ(s.websocket.url, dev.websocket.url);
    EXPECT_EQ(s.websocket.ping_interval_seconds, dev.websocket.ping_interval_seconds);
    EXPECT_EQ(s.storage.data_directory, dev.storage.data_directory);
}

TEST(Settings, FromEnvironmentSelectsProductionPreset) {
    setenv("MDE_ENV", "production", 1);
    unsetenv("MDE_WEBSOCKET_URL");
    unsetenv("MDE_PING_INTERVAL");
    unsetenv("MDE_STORAGE_BACKEND");
    unsetenv("MDE_DATA_DIRECTORY");

    auto s = Settings::from_environment();
    auto prod = Settings::production();
    EXPECT_EQ(s.websocket.ping_interval_seconds, prod.websocket.ping_interval_seconds);
    EXPECT_EQ(s.storage.backend, "parquet");
    EXPECT_EQ(s.storage.data_directory, prod.storage.data_directory);
    EXPECT_EQ(s.storage.write_buffer_size, prod.storage.write_buffer_size);

    unsetenv("MDE_ENV");
}

TEST(Settings, FromEnvironmentReadsEnvVars) {
    unsetenv("MDE_ENV");
    setenv("MDE_WEBSOCKET_URL", "ws://localhost:8080", 1);
    setenv("MDE_PING_INTERVAL", "10", 1);
    setenv("MDE_GAMMA_API_URL", "http://localhost:3000", 1);
    setenv("MDE_SNAPSHOT_INTERVAL", "3", 1);
    setenv("MDE_STORAGE_BACKEND", "parquet", 1);
    setenv("MDE_DATA_DIRECTORY", "/tmp/mde", 1);
    setenv("MDE_WRITE_BUFFER_SIZE", "2048", 1);

    auto s = Settings::from_environment();
    EXPECT_EQ(s.websocket.url, "ws://localhost:8080");
    EXPECT_EQ(s.websocket.ping_interval_seconds, 10);
    EXPECT_EQ(s.api.gamma_api_base_url, "http://localhost:3000");
    EXPECT_EQ(s.service.snapshot_interval_seconds, 3);
    EXPECT_EQ(s.storage.backend, "parquet");
    EXPECT_EQ(s.storage.data_directory, "/tmp/mde");
    EXPECT_EQ(s.storage.write_buffer_size, 2048);

    // Clean up
    unsetenv("MDE_WEBSOCKET_URL");
    unsetenv("MDE_PING_INTERVAL");
    unsetenv("MDE_GAMMA_API_URL");
    unsetenv("MDE_SNAPSHOT_INTERVAL");
    unsetenv("MDE_STORAGE_BACKEND");
    unsetenv("MDE_DATA_DIRECTORY");
    unsetenv("MDE_WRITE_BUFFER_SIZE");
}

TEST(Settings, FromEnvironmentHandlesInvalidInt) {
    unsetenv("MDE_ENV");
    setenv("MDE_PING_INTERVAL", "not_a_number", 1);
    auto s = Settings::from_environment();
    EXPECT_EQ(s.websocket.ping_interval_seconds, 30);  // Falls back to dev preset default
    unsetenv("MDE_PING_INTERVAL");
}

TEST(Settings, DevelopmentPreset) {
    auto s = Settings::development();
    EXPECT_EQ(s.websocket.ping_interval_seconds, 30);
    EXPECT_EQ(s.service.snapshot_interval_seconds, 10);
    EXPECT_EQ(s.storage.backend, "memory");
    EXPECT_EQ(s.storage.data_directory, "data/dev");
}

TEST(Settings, ProductionPreset) {
    auto s = Settings::production();
    EXPECT_EQ(s.websocket.ping_interval_seconds, 15);
    EXPECT_EQ(s.service.snapshot_interval_seconds, 5);
    EXPECT_EQ(s.storage.backend, "parquet");
    EXPECT_EQ(s.storage.data_directory, "data/prod");
    EXPECT_EQ(s.storage.write_buffer_size, 4096);
    EXPECT_TRUE(s.discovery.enabled);
}

TEST(Settings, DiscoverySettingsFromEnvVars) {
    unsetenv("MDE_ENV");
    setenv("MDE_DISCOVERY_ENABLED", "true", 1);
    setenv("MDE_MAX_TRACKED_MARKETS", "100", 1);
    setenv("MDE_DISCOVERY_INTERVAL", "600", 1);
    setenv("MDE_MARKETS_PER_POLL", "25", 1);

    auto s = Settings::from_environment();
    EXPECT_TRUE(s.discovery.enabled);
    EXPECT_EQ(s.discovery.max_tracked_markets, 100);
    EXPECT_EQ(s.discovery.discovery_interval_seconds, 600);
    EXPECT_EQ(s.discovery.markets_per_poll, 25);

    unsetenv("MDE_DISCOVERY_ENABLED");
    unsetenv("MDE_MAX_TRACKED_MARKETS");
    unsetenv("MDE_DISCOVERY_INTERVAL");
    unsetenv("MDE_MARKETS_PER_POLL");
}

TEST(Settings, DiscoveryDisabledByDefault) {
    unsetenv("MDE_ENV");
    unsetenv("MDE_DISCOVERY_ENABLED");

    auto s = Settings::from_environment();
    EXPECT_FALSE(s.discovery.enabled);
}
