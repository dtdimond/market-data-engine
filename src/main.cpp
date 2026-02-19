#include "config/Settings.hpp"
#include "infrastructure/PolymarketClient.hpp"
#include "repositories/IOrderBookRepository.hpp"
#include "repositories/InMemoryOrderBookRepository.hpp"
#include "services/OrderBookService.hpp"

#ifdef MDE_HAS_PARQUET
#include "repositories/parquet/ParquetOrderBookRepository.hpp"
#endif

#include <ixwebsocket/IXHttpClient.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>

static std::atomic<bool> running{true};

void signal_handler(int) {
    running = false;
}

std::string fetch_market_name(const std::string& token_id,
                              const mde::config::ApiSettings& api_settings) {
    ix::HttpClient client;
    auto args = client.createRequest();
    args->connectTimeout = 5;
    args->transferTimeout = 10;

    std::string url =
        api_settings.gamma_api_base_url + "/markets?clob_token_ids=" + token_id;
    auto response = client.get(url, args);

    if (response->statusCode == 200) {
        auto json = nlohmann::json::parse(response->body, nullptr, false);
        if (json.is_array() && !json.empty() && json[0].contains("question")) {
            return json[0]["question"].get<std::string>();
        }
    }
    return "";
}

void print_book(const mde::domain::OrderBook& book, const std::string& market_name) {
    std::cout << "\033[2J\033[H";  // Clear screen

    std::cout << "=== Market Data Engine ===" << std::endl;
    if (!market_name.empty()) {
        std::cout << "Market: " << market_name << std::endl;
    }
    std::cout << "Asset: " << book.get_asset().token_id().substr(0, 20) << "..." << std::endl;
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Seq: " << book.get_last_sequence_number()
              << "  Tick: " << book.get_tick_size().value() << std::endl;
    std::cout << std::endl;

    if (book.get_depth() > 0) {
        auto spread = book.get_spread();
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "Best Bid: " << spread.best_bid.value()
                  << "  Best Ask: " << spread.best_ask.value()
                  << "  Spread: " << spread.value() << std::endl;
        std::cout << "Midpoint: " << book.get_midpoint().value() << std::endl;
        std::cout << std::endl;

        // Top 5 bids and asks
        auto& bids = book.get_bids();
        auto& asks = book.get_asks();
        int levels = 5;

        std::cout << "       BIDS              ASKS" << std::endl;
        std::cout << "  Price    Size      Price    Size" << std::endl;
        std::cout << "  -----    ----      -----    ----" << std::endl;

        for (int i = 0; i < levels; ++i) {
            if (i < static_cast<int>(bids.size())) {
                std::cout << std::setprecision(4) << std::setw(7) << bids[i].price().value()
                          << std::setprecision(1) << std::setw(8) << bids[i].size().size();
            } else {
                std::cout << "               ";
            }

            std::cout << "    ";

            if (i < static_cast<int>(asks.size())) {
                std::cout << std::setw(7) << std::setprecision(4) << asks[i].price().value()
                          << std::setw(8) << std::setprecision(1) << asks[i].size().size();
            }
            std::cout << std::endl;
        }

        std::cout << std::endl;
        std::cout << "Depth: " << bids.size() << " bids, " << asks.size() << " asks" << std::endl;
    } else {
        std::cout << "(waiting for data...)" << std::endl;
    }

    if (book.get_latest_trade().has_value()) {
        auto trade = *book.get_latest_trade();
        std::cout << std::endl;
        std::cout << "Last Trade: " << std::fixed << std::setprecision(4) << trade.price.value()
                  << " x " << std::setprecision(2) << trade.size.size()
                  << " (" << (trade.side == mde::domain::Side::BUY ? "BUY" : "SELL") << ")"
                  << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: market_data_engine <token_id>" << std::endl;
        return 1;
    }

    std::string token_id = argv[1];

    auto settings = mde::config::Settings::from_environment();

    std::unique_ptr<mde::repositories::IOrderBookRepository> repo;
    if (settings.storage.backend == "parquet") {
#ifdef MDE_HAS_PARQUET
        auto fs = mde::repositories::pq::ParquetOrderBookRepository::make_local_fs(
            settings.storage.data_directory);
        repo = std::make_unique<mde::repositories::pq::ParquetOrderBookRepository>(
            fs, settings.storage);
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

    service.subscribe(token_id);

    std::cout << "Looking up market..." << std::endl;
    std::string market_name = fetch_market_name(token_id, settings.api);

    std::signal(SIGINT, signal_handler);

    service.start();
    std::cout << "Connecting..." << std::endl;

    // We need the full MarketAsset (condition_id + token_id) to look up the book,
    // but we only know token_id until the first event arrives with the condition_id.
    std::optional<mde::domain::MarketAsset> resolved_asset;

    while (running) {
        // sleep to only update display every 1 sec. Book is updating in other thread as events come in.
        std::this_thread::sleep_for(std::chrono::seconds(1));

        try {
            if (!resolved_asset) {
                resolved_asset = service.resolve_asset(token_id);
            }

            if (resolved_asset) {
                auto& book = service.get_current_book(*resolved_asset);
                print_book(book, market_name);
            }
        } catch (...) {
            // Book not ready yet
        }
    }

    service.stop();
    std::cout << "\nDone. Processed " << service.event_count() << " events." << std::endl;
    return 0;
}
