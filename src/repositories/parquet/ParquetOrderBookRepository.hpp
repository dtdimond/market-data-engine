#pragma once

#include "config/Settings.hpp"
#include "repositories/IOrderBookRepository.hpp"

#include <arrow/filesystem/api.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace mde::repositories::pq {

class ParquetOrderBookRepository : public mde::repositories::IOrderBookRepository {
public:
    ParquetOrderBookRepository(std::shared_ptr<arrow::fs::FileSystem> fs,
                               const mde::config::StorageSettings& settings);

    /// Create a local filesystem rooted at root_dir (creates dir if needed).
    static std::shared_ptr<arrow::fs::FileSystem> make_local_fs(const std::string& root_dir);

    /// Create an S3-compatible filesystem (AWS S3, R2, B2, Wasabi, MinIO).
    /// Requires arrow::fs::EnsureS3Initialized() before use.
    static std::shared_ptr<arrow::fs::FileSystem> make_s3_fs(
        const mde::config::StorageSettings& settings);
    ~ParquetOrderBookRepository() override;

    // IOrderBookRepository
    void append_event(const mde::domain::OrderBookEventVariant& event) override;
    std::vector<mde::domain::OrderBookEventVariant> get_events_since(
        const mde::domain::MarketAsset& asset, uint64_t sequence_number) const override;
    void store_snapshot(const mde::domain::OrderBook& book) override;
    std::optional<mde::domain::OrderBook> get_latest_snapshot(
        const mde::domain::MarketAsset& asset) const override;

private:
    void maybe_flush();
    void flush();
    void flush_buffer(const std::string& event_type,
                      const std::vector<mde::domain::OrderBookEventVariant>& events);

    // File path helpers
    std::string events_dir(const std::string& event_type,
                           const std::string& token_id) const;
    std::string snapshot_path(const std::string& token_id) const;
    static std::string token_prefix(const std::string& token_id);
    static std::string token_hash(const std::string& token_id);
    static std::string date_string(int64_t timestamp_ms);
    static std::string hour_string(int64_t timestamp_ms);

    // Parquet read/write
    void write_book_snapshots(const std::string& path,
                              const std::vector<mde::domain::OrderBookEventVariant>& events);
    void write_book_deltas(const std::string& path,
                           const std::vector<mde::domain::OrderBookEventVariant>& events);
    void write_trade_events(const std::string& path,
                            const std::vector<mde::domain::OrderBookEventVariant>& events);
    void write_tick_size_changes(const std::string& path,
                                 const std::vector<mde::domain::OrderBookEventVariant>& events);

    std::vector<mde::domain::OrderBookEventVariant> read_events_from_directory(
        const std::string& dir, const mde::domain::MarketAsset& asset,
        uint64_t min_sequence) const;

    std::shared_ptr<arrow::fs::FileSystem> fs_;
    mde::config::StorageSettings settings_;
    mutable std::mutex mutex_;

    // Buffers per event type
    std::vector<mde::domain::OrderBookEventVariant> snapshot_buffer_;
    std::vector<mde::domain::OrderBookEventVariant> delta_buffer_;
    std::vector<mde::domain::OrderBookEventVariant> trade_buffer_;
    std::vector<mde::domain::OrderBookEventVariant> tick_size_buffer_;

    std::chrono::steady_clock::time_point last_flush_time_;
    uint64_t min_seq_in_buffer_{0};
    uint64_t max_seq_in_buffer_{0};
};

} // namespace mde::repositories::pq
