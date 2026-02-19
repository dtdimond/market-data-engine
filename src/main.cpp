#include "config/Settings.hpp"
#include "infrastructure/PolymarketClient.hpp"
#include "repositories/IOrderBookRepository.hpp"
#include "repositories/InMemoryOrderBookRepository.hpp"
#include "services/OrderBookService.hpp"

#ifdef MDE_HAS_PARQUET
#include "infrastructure/MarketDiscovery.hpp"
#include "repositories/parquet/ParquetOrderBookRepository.hpp"
#include <arrow/filesystem/localfs.h>
#include <arrow/filesystem/s3fs.h>
#endif

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

static std::atomic<bool> running{true};

void signal_handler(int) {
    running = false;
}

int main(int argc, char* argv[]) {
    auto settings = mde::config::Settings::from_environment();

    // Optional CLI arg: seed token ID
    std::string seed_token_id;
    if (argc >= 2) {
        seed_token_id = argv[1];
    }

    // If no seed token and discovery disabled, nothing to do
    if (seed_token_id.empty() && !settings.discovery.enabled) {
        std::cerr << "Usage: market_data_engine [token_id]" << std::endl;
        std::cerr << "       Set MDE_DISCOVERY_ENABLED=true for auto-discovery mode." << std::endl;
        return 1;
    }

    std::unique_ptr<mde::repositories::IOrderBookRepository> repo;

#ifdef MDE_HAS_PARQUET
    struct S3Guard {
        S3Guard() { (void)arrow::fs::EnsureS3Initialized(); }
        ~S3Guard() { (void)arrow::fs::EnsureS3Finalized(); }
    };
    std::unique_ptr<S3Guard> s3_guard;

    // Shared filesystem for both repo and discovery
    std::shared_ptr<arrow::fs::FileSystem> shared_fs;
#endif

    if (settings.storage.backend == "s3") {
#ifdef MDE_HAS_PARQUET
        if (settings.storage.s3_bucket.empty()) {
            std::cerr << "S3 backend requires MDE_S3_BUCKET." << std::endl;
            return 1;
        }
        s3_guard = std::make_unique<S3Guard>();
        shared_fs = mde::repositories::pq::ParquetOrderBookRepository::make_s3_fs(
            settings.storage);
        repo = std::make_unique<mde::repositories::pq::ParquetOrderBookRepository>(
            shared_fs, settings.storage);
#else
        std::cerr << "S3 backend requested but not compiled in. "
                  << "Rebuild with Apache Arrow installed." << std::endl;
        return 1;
#endif
    } else if (settings.storage.backend == "parquet") {
#ifdef MDE_HAS_PARQUET
        shared_fs = mde::repositories::pq::ParquetOrderBookRepository::make_local_fs(
            settings.storage.data_directory);
        repo = std::make_unique<mde::repositories::pq::ParquetOrderBookRepository>(
            shared_fs, settings.storage);
#else
        std::cerr << "Parquet backend requested but not compiled in. "
                  << "Rebuild with Apache Arrow installed." << std::endl;
        return 1;
#endif
    } else {
        repo = std::make_unique<mde::repositories::InMemoryOrderBookRepository>();
    }

    mde::infrastructure::PolymarketClient client(settings.websocket);
    mde::services::OrderBookService service(*repo, client, settings.service.snapshot_interval_seconds);

    // Subscribe seed token if provided
    if (!seed_token_id.empty()) {
        service.subscribe(seed_token_id);
    }

#ifdef MDE_HAS_PARQUET
    // Discovery setup
    std::unique_ptr<mde::infrastructure::MarketDiscovery> discovery;
    if (settings.discovery.enabled) {
        discovery = std::make_unique<mde::infrastructure::MarketDiscovery>(
            shared_fs, settings.api, settings.discovery);
        discovery->load();

        // Subscribe all restored tracked IDs
        for (const auto& id : discovery->tracked_token_ids()) {
            service.subscribe(id);
        }

        std::cout << "[discovery] Restored " << discovery->tracked_count()
                  << " tracked markets" << std::endl;
    }
#endif

    std::signal(SIGINT, signal_handler);

    service.start();
    std::cout << "[engine] Started" << std::endl;

#ifdef MDE_HAS_PARQUET
    // Discovery thread
    std::thread discovery_thread;
    if (discovery) {
        discovery_thread = std::thread([&]() {
            while (running) {
                try {
                    size_t added = discovery->poll([&](const std::vector<std::string>& new_ids) {
                        for (const auto& id : new_ids) {
                            service.subscribe(id);
                        }
                    });
                    if (added > 0) {
                        std::cout << "[discovery] Added " << added << " new markets, total="
                                  << discovery->tracked_count() << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[discovery] Poll error: " << e.what() << std::endl;
                }

                // Sleep in 1-second increments to allow clean shutdown
                for (int i = 0; i < settings.discovery.discovery_interval_seconds && running; ++i) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
        });
    }
#endif

    // Log-mode stats loop
    uint64_t last_event_count = 0;
    auto last_stats_time = std::chrono::steady_clock::now();

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        if (!running) break;

        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - last_stats_time).count();
        uint64_t current_events = service.event_count();
        double events_per_sec = (elapsed > 0) ? (current_events - last_event_count) / elapsed : 0;

        std::cout << "[stats] markets=" << service.book_count()
                  << " events/sec=" << static_cast<int>(events_per_sec)
                  << " total_events=" << current_events
                  << std::endl;

        last_event_count = current_events;
        last_stats_time = now;
    }

    service.stop();

#ifdef MDE_HAS_PARQUET
    if (discovery_thread.joinable()) {
        discovery_thread.join();
    }
#endif

    std::cout << "\n[engine] Done. Processed " << service.event_count() << " events." << std::endl;
    return 0;
}
