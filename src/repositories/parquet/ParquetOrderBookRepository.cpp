#include "repositories/parquet/ParquetOrderBookRepository.hpp"
#include "repositories/parquet/ParquetSchemas.hpp"

#include <arrow/api.h>
#include <arrow/filesystem/api.h>
#include <arrow/filesystem/localfs.h>
#include <arrow/filesystem/s3fs.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <variant>

using namespace mde::domain;

namespace mde::repositories::pq {

namespace {

// Helper to get sequence_number from any event variant
uint64_t get_seq(const OrderBookEventVariant& event) {
    return std::visit([](const auto& e) { return e.sequence_number; }, event);
}

const MarketAsset& get_asset(const OrderBookEventVariant& event) {
    return std::visit(
        [](const auto& e) -> const MarketAsset& { return e.asset; }, event);
}

int64_t get_timestamp_ms(const OrderBookEventVariant& event) {
    return std::visit(
        [](const auto& e) { return e.timestamp.milliseconds(); }, event);
}

bool ends_with(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Extract the first path component (event type) from a relative path like
// "book_snapshot/6581861/2025-07-15/file.parquet"
std::string first_path_component(const std::string& path) {
    auto pos = path.find('/');
    if (pos == std::string::npos) return path;
    return path.substr(0, pos);
}

// Extract parent directory from a path string
std::string parent_path(const std::string& path) {
    auto pos = path.rfind('/');
    if (pos == std::string::npos) return "";
    return path.substr(0, pos);
}

// Extract filename stem from a path string (no directory, no extension)
std::string stem(const std::string& path) {
    auto slash = path.rfind('/');
    std::string filename = (slash == std::string::npos) ? path : path.substr(slash + 1);
    auto dot = filename.rfind('.');
    if (dot == std::string::npos) return filename;
    return filename.substr(0, dot);
}

} // namespace

ParquetOrderBookRepository::ParquetOrderBookRepository(
    std::shared_ptr<arrow::fs::FileSystem> fs,
    const mde::config::StorageSettings& settings)
    : fs_(std::move(fs))
    , settings_(settings)
    , last_flush_time_(std::chrono::steady_clock::now()) {
}

ParquetOrderBookRepository::~ParquetOrderBookRepository() {
    std::lock_guard lock(mutex_);
    flush();
}

std::shared_ptr<arrow::fs::FileSystem> ParquetOrderBookRepository::make_local_fs(
    const std::string& root_dir) {
    auto local = std::make_shared<arrow::fs::LocalFileSystem>();
    (void)local->CreateDir(root_dir, /*recursive=*/true);
    return std::make_shared<arrow::fs::SubTreeFileSystem>(root_dir, local);
}

std::shared_ptr<arrow::fs::FileSystem> ParquetOrderBookRepository::make_s3_fs(
    const mde::config::StorageSettings& settings) {
    auto options = arrow::fs::S3Options::Defaults();
    options.region = settings.s3_region;
    options.scheme = settings.s3_scheme;
    if (!settings.s3_endpoint_override.empty()) {
        options.endpoint_override = settings.s3_endpoint_override;
    }

    auto s3fs = arrow::fs::S3FileSystem::Make(options).ValueOrDie();
    std::string base_path = settings.s3_bucket;
    if (!settings.s3_prefix.empty()) {
        base_path += "/" + settings.s3_prefix;
    }
    return std::make_shared<arrow::fs::SubTreeFileSystem>(base_path, s3fs);
}

void ParquetOrderBookRepository::append_event(const OrderBookEventVariant& event) {
    std::lock_guard lock(mutex_);

    auto seq = get_seq(event);
    if (min_seq_in_buffer_ == 0) min_seq_in_buffer_ = seq;
    max_seq_in_buffer_ = seq;

    std::visit([this, &event](const auto& e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, BookSnapshot>) {
            snapshot_buffer_.push_back(event);
        } else if constexpr (std::is_same_v<T, BookDelta>) {
            delta_buffer_.push_back(event);
        } else if constexpr (std::is_same_v<T, TradeEvent>) {
            trade_buffer_.push_back(event);
        } else if constexpr (std::is_same_v<T, TickSizeChange>) {
            tick_size_buffer_.push_back(event);
        }
    }, event);

    maybe_flush();
}

void ParquetOrderBookRepository::maybe_flush() {
    size_t total = snapshot_buffer_.size() + delta_buffer_.size() +
                   trade_buffer_.size() + tick_size_buffer_.size();

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_flush_time_).count();

    if (total >= static_cast<size_t>(settings_.write_buffer_size) || elapsed >= 30) {
        flush();
    }
}

void ParquetOrderBookRepository::flush() {
    if (!snapshot_buffer_.empty()) {
        flush_buffer("book_snapshot", snapshot_buffer_);
        snapshot_buffer_.clear();
    }
    if (!delta_buffer_.empty()) {
        flush_buffer("book_delta", delta_buffer_);
        delta_buffer_.clear();
    }
    if (!trade_buffer_.empty()) {
        flush_buffer("trade_event", trade_buffer_);
        trade_buffer_.clear();
    }
    if (!tick_size_buffer_.empty()) {
        flush_buffer("tick_size_change", tick_size_buffer_);
        tick_size_buffer_.clear();
    }
    min_seq_in_buffer_ = 0;
    max_seq_in_buffer_ = 0;
    last_flush_time_ = std::chrono::steady_clock::now();
}

void ParquetOrderBookRepository::flush_buffer(
    const std::string& event_type,
    const std::vector<OrderBookEventVariant>& events) {
    if (events.empty()) return;

    // Use the first event's token_id and timestamp for directory structure
    const auto& first_asset = get_asset(events.front());
    auto first_ts = get_timestamp_ms(events.front());

    uint64_t seq_start = get_seq(events.front());
    uint64_t seq_end = get_seq(events.back());

    std::string dir = events_dir(event_type, first_asset.token_id()) + "/"
        + date_string(first_ts);
    (void)fs_->CreateDir(dir, /*recursive=*/true);

    std::string filename = event_type + "_" + hour_string(first_ts) + "_"
        + std::to_string(seq_start) + "_" + std::to_string(seq_end) + ".parquet";
    std::string path = dir + "/" + filename;

    if (event_type == "book_snapshot") {
        write_book_snapshots(path, events);
    } else if (event_type == "book_delta") {
        write_book_deltas(path, events);
    } else if (event_type == "trade_event") {
        write_trade_events(path, events);
    } else if (event_type == "tick_size_change") {
        write_tick_size_changes(path, events);
    }
}

// --- Write helpers ---

void ParquetOrderBookRepository::write_book_snapshots(
    const std::string& path,
    const std::vector<OrderBookEventVariant>& events) {

    auto schema = ParquetSchemas::book_snapshot_schema();

    arrow::StringBuilder condition_id_builder, token_id_builder, hash_builder;
    arrow::Int64Builder timestamp_builder;
    arrow::UInt64Builder seq_builder;

    auto bid_prices_builder = std::make_shared<arrow::DoubleBuilder>();
    auto bid_sizes_builder = std::make_shared<arrow::DoubleBuilder>();
    auto ask_prices_builder = std::make_shared<arrow::DoubleBuilder>();
    auto ask_sizes_builder = std::make_shared<arrow::DoubleBuilder>();

    arrow::ListBuilder bid_prices_list(arrow::default_memory_pool(), bid_prices_builder);
    arrow::ListBuilder bid_sizes_list(arrow::default_memory_pool(), bid_sizes_builder);
    arrow::ListBuilder ask_prices_list(arrow::default_memory_pool(), ask_prices_builder);
    arrow::ListBuilder ask_sizes_list(arrow::default_memory_pool(), ask_sizes_builder);

    for (const auto& event : events) {
        const auto& snap = std::get<BookSnapshot>(event);
        (void)condition_id_builder.Append(snap.asset.condition_id());
        (void)token_id_builder.Append(snap.asset.token_id());
        (void)timestamp_builder.Append(snap.timestamp.milliseconds());
        (void)seq_builder.Append(snap.sequence_number);
        (void)hash_builder.Append(snap.hash);

        (void)bid_prices_list.Append();
        (void)bid_sizes_list.Append();
        for (const auto& bid : snap.bids) {
            (void)bid_prices_builder->Append(bid.price().value());
            (void)bid_sizes_builder->Append(bid.size().size());
        }

        (void)ask_prices_list.Append();
        (void)ask_sizes_list.Append();
        for (const auto& ask : snap.asks) {
            (void)ask_prices_builder->Append(ask.price().value());
            (void)ask_sizes_builder->Append(ask.size().size());
        }
    }

    std::shared_ptr<arrow::Array> arr_cid, arr_tid, arr_ts, arr_seq, arr_hash;
    std::shared_ptr<arrow::Array> arr_bp, arr_bs, arr_ap, arr_as;
    (void)condition_id_builder.Finish(&arr_cid);
    (void)token_id_builder.Finish(&arr_tid);
    (void)timestamp_builder.Finish(&arr_ts);
    (void)seq_builder.Finish(&arr_seq);
    (void)hash_builder.Finish(&arr_hash);
    (void)bid_prices_list.Finish(&arr_bp);
    (void)bid_sizes_list.Finish(&arr_bs);
    (void)ask_prices_list.Finish(&arr_ap);
    (void)ask_sizes_list.Finish(&arr_as);

    auto table = arrow::Table::Make(schema,
        {arr_cid, arr_tid, arr_ts, arr_seq, arr_hash, arr_bp, arr_bs, arr_ap, arr_as});

    auto outfile = fs_->OpenOutputStream(path).ValueOrDie();
    (void)::parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile, events.size());
    (void)outfile->Close();
}

void ParquetOrderBookRepository::write_book_deltas(
    const std::string& path,
    const std::vector<OrderBookEventVariant>& events) {

    auto schema = ParquetSchemas::book_delta_schema();

    arrow::StringBuilder condition_id_builder, token_id_builder;
    arrow::Int64Builder timestamp_builder;
    arrow::UInt64Builder seq_builder;

    auto asset_ids_inner = std::make_shared<arrow::StringBuilder>();
    auto prices_inner = std::make_shared<arrow::DoubleBuilder>();
    auto sizes_inner = std::make_shared<arrow::DoubleBuilder>();
    auto sides_inner = std::make_shared<arrow::UInt8Builder>();
    auto best_bids_inner = std::make_shared<arrow::DoubleBuilder>();
    auto best_asks_inner = std::make_shared<arrow::DoubleBuilder>();

    arrow::ListBuilder asset_ids_list(arrow::default_memory_pool(), asset_ids_inner);
    arrow::ListBuilder prices_list(arrow::default_memory_pool(), prices_inner);
    arrow::ListBuilder sizes_list(arrow::default_memory_pool(), sizes_inner);
    arrow::ListBuilder sides_list(arrow::default_memory_pool(), sides_inner);
    arrow::ListBuilder best_bids_list(arrow::default_memory_pool(), best_bids_inner);
    arrow::ListBuilder best_asks_list(arrow::default_memory_pool(), best_asks_inner);

    for (const auto& event : events) {
        const auto& delta = std::get<BookDelta>(event);
        (void)condition_id_builder.Append(delta.asset.condition_id());
        (void)token_id_builder.Append(delta.asset.token_id());
        (void)timestamp_builder.Append(delta.timestamp.milliseconds());
        (void)seq_builder.Append(delta.sequence_number);

        (void)asset_ids_list.Append();
        (void)prices_list.Append();
        (void)sizes_list.Append();
        (void)sides_list.Append();
        (void)best_bids_list.Append();
        (void)best_asks_list.Append();
        for (const auto& change : delta.changes) {
            (void)asset_ids_inner->Append(change.asset_id);
            (void)prices_inner->Append(change.price.value());
            (void)sizes_inner->Append(change.new_size.size());
            (void)sides_inner->Append(static_cast<uint8_t>(change.side));
            (void)best_bids_inner->Append(change.best_bid.value());
            (void)best_asks_inner->Append(change.best_ask.value());
        }
    }

    std::shared_ptr<arrow::Array> arr_cid, arr_tid, arr_ts, arr_seq;
    std::shared_ptr<arrow::Array> arr_aids, arr_prices, arr_sizes, arr_sides, arr_bbids, arr_basks;
    (void)condition_id_builder.Finish(&arr_cid);
    (void)token_id_builder.Finish(&arr_tid);
    (void)timestamp_builder.Finish(&arr_ts);
    (void)seq_builder.Finish(&arr_seq);
    (void)asset_ids_list.Finish(&arr_aids);
    (void)prices_list.Finish(&arr_prices);
    (void)sizes_list.Finish(&arr_sizes);
    (void)sides_list.Finish(&arr_sides);
    (void)best_bids_list.Finish(&arr_bbids);
    (void)best_asks_list.Finish(&arr_basks);

    auto table = arrow::Table::Make(schema,
        {arr_cid, arr_tid, arr_ts, arr_seq, arr_aids, arr_prices, arr_sizes, arr_sides, arr_bbids, arr_basks});

    auto outfile = fs_->OpenOutputStream(path).ValueOrDie();
    (void)::parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile, events.size());
    (void)outfile->Close();
}

void ParquetOrderBookRepository::write_trade_events(
    const std::string& path,
    const std::vector<OrderBookEventVariant>& events) {

    auto schema = ParquetSchemas::trade_event_schema();

    arrow::StringBuilder condition_id_builder, token_id_builder, fee_builder;
    arrow::Int64Builder timestamp_builder;
    arrow::UInt64Builder seq_builder;
    arrow::DoubleBuilder price_builder, size_builder;
    arrow::UInt8Builder side_builder;

    for (const auto& event : events) {
        const auto& trade = std::get<TradeEvent>(event);
        (void)condition_id_builder.Append(trade.asset.condition_id());
        (void)token_id_builder.Append(trade.asset.token_id());
        (void)timestamp_builder.Append(trade.timestamp.milliseconds());
        (void)seq_builder.Append(trade.sequence_number);
        (void)price_builder.Append(trade.price.value());
        (void)size_builder.Append(trade.size.size());
        (void)side_builder.Append(static_cast<uint8_t>(trade.side));
        (void)fee_builder.Append(trade.fee_rate_bps);
    }

    std::shared_ptr<arrow::Array> arr_cid, arr_tid, arr_ts, arr_seq;
    std::shared_ptr<arrow::Array> arr_price, arr_size, arr_side, arr_fee;
    (void)condition_id_builder.Finish(&arr_cid);
    (void)token_id_builder.Finish(&arr_tid);
    (void)timestamp_builder.Finish(&arr_ts);
    (void)seq_builder.Finish(&arr_seq);
    (void)price_builder.Finish(&arr_price);
    (void)size_builder.Finish(&arr_size);
    (void)side_builder.Finish(&arr_side);
    (void)fee_builder.Finish(&arr_fee);

    auto table = arrow::Table::Make(schema,
        {arr_cid, arr_tid, arr_ts, arr_seq, arr_price, arr_size, arr_side, arr_fee});

    auto outfile = fs_->OpenOutputStream(path).ValueOrDie();
    (void)::parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile, events.size());
    (void)outfile->Close();
}

void ParquetOrderBookRepository::write_tick_size_changes(
    const std::string& path,
    const std::vector<OrderBookEventVariant>& events) {

    auto schema = ParquetSchemas::tick_size_change_schema();

    arrow::StringBuilder condition_id_builder, token_id_builder;
    arrow::Int64Builder timestamp_builder;
    arrow::UInt64Builder seq_builder;
    arrow::DoubleBuilder old_tick_builder, new_tick_builder;

    for (const auto& event : events) {
        const auto& tick = std::get<TickSizeChange>(event);
        (void)condition_id_builder.Append(tick.asset.condition_id());
        (void)token_id_builder.Append(tick.asset.token_id());
        (void)timestamp_builder.Append(tick.timestamp.milliseconds());
        (void)seq_builder.Append(tick.sequence_number);
        (void)old_tick_builder.Append(tick.old_tick_size.value());
        (void)new_tick_builder.Append(tick.new_tick_size.value());
    }

    std::shared_ptr<arrow::Array> arr_cid, arr_tid, arr_ts, arr_seq;
    std::shared_ptr<arrow::Array> arr_old, arr_new;
    (void)condition_id_builder.Finish(&arr_cid);
    (void)token_id_builder.Finish(&arr_tid);
    (void)timestamp_builder.Finish(&arr_ts);
    (void)seq_builder.Finish(&arr_seq);
    (void)old_tick_builder.Finish(&arr_old);
    (void)new_tick_builder.Finish(&arr_new);

    auto table = arrow::Table::Make(schema,
        {arr_cid, arr_tid, arr_ts, arr_seq, arr_old, arr_new});

    auto outfile = fs_->OpenOutputStream(path).ValueOrDie();
    (void)::parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile, events.size());
    (void)outfile->Close();
}

// --- Read path ---

std::vector<OrderBookEventVariant> ParquetOrderBookRepository::get_events_since(
    const MarketAsset& asset, uint64_t sequence_number) const {
    std::lock_guard lock(mutex_);

    std::vector<OrderBookEventVariant> result;

    // Read from storage for each event type
    const std::string event_types[] = {"book_snapshot", "book_delta", "trade_event", "tick_size_change"};
    for (const auto& event_type : event_types) {
        std::string dir = events_dir(event_type, asset.token_id());
        auto disk_events = read_events_from_directory(dir, asset, sequence_number);
        result.insert(result.end(), disk_events.begin(), disk_events.end());
    }

    // Merge with unflushed buffers
    auto merge_buffer = [&](const std::vector<OrderBookEventVariant>& buffer) {
        for (const auto& event : buffer) {
            if (get_asset(event) == asset && get_seq(event) > sequence_number) {
                result.push_back(event);
            }
        }
    };
    merge_buffer(snapshot_buffer_);
    merge_buffer(delta_buffer_);
    merge_buffer(trade_buffer_);
    merge_buffer(tick_size_buffer_);

    // Sort by sequence number
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return get_seq(a) < get_seq(b);
    });

    return result;
}

std::vector<OrderBookEventVariant> ParquetOrderBookRepository::read_events_from_directory(
    const std::string& dir, const MarketAsset& asset, uint64_t min_sequence) const {

    std::vector<OrderBookEventVariant> result;

    arrow::fs::FileSelector selector;
    selector.base_dir = dir;
    selector.allow_not_found = true;
    selector.recursive = true;
    auto listing_result = fs_->GetFileInfo(selector);
    if (!listing_result.ok()) return result;
    auto listing = listing_result.ValueOrDie();

    for (const auto& file_info : listing) {
        if (file_info.type() != arrow::fs::FileType::File) continue;
        if (!ends_with(file_info.path(), ".parquet")) continue;

        // Try to extract seq range from filename to skip files entirely
        std::string filename = stem(file_info.path());
        // Format: {event_type}_{HH}_{seq_start}_{seq_end}
        // Quick check: if seq_end < min_sequence, skip this file
        auto last_underscore = filename.rfind('_');
        if (last_underscore != std::string::npos) {
            auto second_last = filename.rfind('_', last_underscore - 1);
            if (second_last != std::string::npos) {
                try {
                    uint64_t seq_end = std::stoull(filename.substr(last_underscore + 1));
                    if (seq_end <= min_sequence) continue;
                } catch (...) {}
            }
        }

        // Read the parquet file
        auto infile = fs_->OpenInputFile(file_info.path()).ValueOrDie();
        std::unique_ptr<::parquet::arrow::FileReader> reader;
        auto reader_result = ::parquet::arrow::FileReader::Make(arrow::default_memory_pool(), ::parquet::ParquetFileReader::Open(infile));
        if (!reader_result.ok()) continue;
        reader = std::move(reader_result).ValueOrDie();

        std::shared_ptr<arrow::Table> table;
        auto read_status = reader->ReadTable(&table);
        if (!read_status.ok()) continue;

        // Determine event type from the path relative to "events/"
        // file_info.path() within SubTreeFileSystem is like:
        //   "events/book_snapshot/6581861/2025-07-15/file.parquet"
        // We want the component after "events/"
        std::string event_type;
        const std::string events_prefix = "events/";
        auto events_pos = file_info.path().find(events_prefix);
        if (events_pos != std::string::npos) {
            std::string after_events = file_info.path().substr(events_pos + events_prefix.size());
            event_type = first_path_component(after_events);
        }

        auto cid_col = std::static_pointer_cast<arrow::StringArray>(
            table->column(0)->chunk(0));
        auto tid_col = std::static_pointer_cast<arrow::StringArray>(
            table->column(1)->chunk(0));
        auto ts_col = std::static_pointer_cast<arrow::Int64Array>(
            table->column(2)->chunk(0));
        auto seq_col = std::static_pointer_cast<arrow::UInt64Array>(
            table->column(3)->chunk(0));

        for (int64_t i = 0; i < table->num_rows(); ++i) {
            uint64_t seq = seq_col->Value(i);
            if (seq <= min_sequence) continue;

            std::string cid = cid_col->GetString(i);
            std::string tid = tid_col->GetString(i);
            if (tid != asset.token_id() || cid != asset.condition_id()) continue;

            int64_t ts = ts_col->Value(i);
            MarketAsset evt_asset(cid, tid);

            if (event_type == "book_snapshot") {
                BookSnapshot snap{
                    {evt_asset, Timestamp(ts), seq},
                    {}, {},
                    std::static_pointer_cast<arrow::StringArray>(
                        table->column(4)->chunk(0))->GetString(i)
                };

                auto bp_list = std::static_pointer_cast<arrow::ListArray>(
                    table->column(5)->chunk(0));
                auto bs_list = std::static_pointer_cast<arrow::ListArray>(
                    table->column(6)->chunk(0));
                auto ap_list = std::static_pointer_cast<arrow::ListArray>(
                    table->column(7)->chunk(0));
                auto as_list = std::static_pointer_cast<arrow::ListArray>(
                    table->column(8)->chunk(0));

                auto bp_values = std::static_pointer_cast<arrow::DoubleArray>(bp_list->values());
                auto bs_values = std::static_pointer_cast<arrow::DoubleArray>(bs_list->values());

                int32_t bp_start = bp_list->value_offset(i);
                int32_t bp_end = bp_list->value_offset(i + 1);
                for (int32_t j = bp_start; j < bp_end; ++j) {
                    snap.bids.emplace_back(Price(bp_values->Value(j)), Quantity(bs_values->Value(j)));
                }

                auto ap_values = std::static_pointer_cast<arrow::DoubleArray>(ap_list->values());
                auto as_values = std::static_pointer_cast<arrow::DoubleArray>(as_list->values());

                int32_t ap_start = ap_list->value_offset(i);
                int32_t ap_end = ap_list->value_offset(i + 1);
                for (int32_t j = ap_start; j < ap_end; ++j) {
                    snap.asks.emplace_back(Price(ap_values->Value(j)), Quantity(as_values->Value(j)));
                }

                result.push_back(snap);

            } else if (event_type == "book_delta") {
                BookDelta delta{{evt_asset, Timestamp(ts), seq}, {}};

                auto aids_list = std::static_pointer_cast<arrow::ListArray>(
                    table->column(4)->chunk(0));
                auto prices_list = std::static_pointer_cast<arrow::ListArray>(
                    table->column(5)->chunk(0));
                auto sizes_list = std::static_pointer_cast<arrow::ListArray>(
                    table->column(6)->chunk(0));
                auto sides_list = std::static_pointer_cast<arrow::ListArray>(
                    table->column(7)->chunk(0));
                auto bbids_list = std::static_pointer_cast<arrow::ListArray>(
                    table->column(8)->chunk(0));
                auto basks_list = std::static_pointer_cast<arrow::ListArray>(
                    table->column(9)->chunk(0));

                auto aids_values = std::static_pointer_cast<arrow::StringArray>(aids_list->values());
                auto prices_values = std::static_pointer_cast<arrow::DoubleArray>(prices_list->values());
                auto sizes_values = std::static_pointer_cast<arrow::DoubleArray>(sizes_list->values());
                auto sides_values = std::static_pointer_cast<arrow::UInt8Array>(sides_list->values());
                auto bbids_values = std::static_pointer_cast<arrow::DoubleArray>(bbids_list->values());
                auto basks_values = std::static_pointer_cast<arrow::DoubleArray>(basks_list->values());

                int32_t start = aids_list->value_offset(i);
                int32_t end = aids_list->value_offset(i + 1);
                for (int32_t j = start; j < end; ++j) {
                    delta.changes.push_back(PriceLevelDelta{
                        aids_values->GetString(j),
                        Price(prices_values->Value(j)),
                        Quantity(sizes_values->Value(j)),
                        static_cast<Side>(sides_values->Value(j)),
                        Price(bbids_values->Value(j)),
                        Price(basks_values->Value(j)),
                    });
                }

                result.push_back(delta);

            } else if (event_type == "trade_event") {
                TradeEvent trade{
                    {evt_asset, Timestamp(ts), seq},
                    Price(std::static_pointer_cast<arrow::DoubleArray>(
                        table->column(4)->chunk(0))->Value(i)),
                    Quantity(std::static_pointer_cast<arrow::DoubleArray>(
                        table->column(5)->chunk(0))->Value(i)),
                    static_cast<Side>(std::static_pointer_cast<arrow::UInt8Array>(
                        table->column(6)->chunk(0))->Value(i)),
                    std::static_pointer_cast<arrow::StringArray>(
                        table->column(7)->chunk(0))->GetString(i)
                };

                result.push_back(trade);

            } else if (event_type == "tick_size_change") {
                TickSizeChange tick{
                    {evt_asset, Timestamp(ts), seq},
                    Price(std::static_pointer_cast<arrow::DoubleArray>(
                        table->column(4)->chunk(0))->Value(i)),
                    Price(std::static_pointer_cast<arrow::DoubleArray>(
                        table->column(5)->chunk(0))->Value(i))
                };

                result.push_back(tick);
            }
        }
    }

    return result;
}

// --- Snapshot storage ---

void ParquetOrderBookRepository::store_snapshot(const OrderBook& book) {
    std::lock_guard lock(mutex_);

    auto schema = ParquetSchemas::order_book_snapshot_schema();

    arrow::StringBuilder cid_b, tid_b, hash_b, fee_b;
    arrow::Int64Builder ts_b, trade_ts_b;
    arrow::UInt64Builder seq_b;
    arrow::DoubleBuilder tick_b, trade_price_b, trade_size_b;
    arrow::UInt8Builder trade_side_b;
    arrow::BooleanBuilder has_trade_b;

    auto bp_inner = std::make_shared<arrow::DoubleBuilder>();
    auto bs_inner = std::make_shared<arrow::DoubleBuilder>();
    auto ap_inner = std::make_shared<arrow::DoubleBuilder>();
    auto as_inner = std::make_shared<arrow::DoubleBuilder>();
    arrow::ListBuilder bp_list(arrow::default_memory_pool(), bp_inner);
    arrow::ListBuilder bs_list(arrow::default_memory_pool(), bs_inner);
    arrow::ListBuilder ap_list(arrow::default_memory_pool(), ap_inner);
    arrow::ListBuilder as_list(arrow::default_memory_pool(), as_inner);

    (void)cid_b.Append(book.get_asset().condition_id());
    (void)tid_b.Append(book.get_asset().token_id());
    (void)ts_b.Append(book.get_timestamp().milliseconds());
    (void)seq_b.Append(book.get_last_sequence_number());
    (void)tick_b.Append(book.get_tick_size().value());
    (void)hash_b.Append(book.get_book_hash());

    (void)bp_list.Append();
    (void)bs_list.Append();
    for (const auto& bid : book.get_bids()) {
        (void)bp_inner->Append(bid.price().value());
        (void)bs_inner->Append(bid.size().size());
    }

    (void)ap_list.Append();
    (void)as_list.Append();
    for (const auto& ask : book.get_asks()) {
        (void)ap_inner->Append(ask.price().value());
        (void)as_inner->Append(ask.size().size());
    }

    bool has_trade = book.get_latest_trade().has_value();
    (void)has_trade_b.Append(has_trade);

    if (has_trade) {
        auto trade = *book.get_latest_trade();
        (void)trade_price_b.Append(trade.price.value());
        (void)trade_size_b.Append(trade.size.size());
        (void)trade_side_b.Append(static_cast<uint8_t>(trade.side));
        (void)fee_b.Append(trade.fee_rate_bps);
        (void)trade_ts_b.Append(trade.timestamp.milliseconds());
    } else {
        (void)trade_price_b.Append(0.0);
        (void)trade_size_b.Append(0.0);
        (void)trade_side_b.Append(0);
        (void)fee_b.Append("");
        (void)trade_ts_b.Append(0);
    }

    std::shared_ptr<arrow::Array> arr_cid, arr_tid, arr_ts, arr_seq, arr_tick, arr_hash;
    std::shared_ptr<arrow::Array> arr_bp, arr_bs, arr_ap, arr_as;
    std::shared_ptr<arrow::Array> arr_tp, arr_tsz, arr_ts2, arr_fee, arr_ht, arr_tts;
    (void)cid_b.Finish(&arr_cid);
    (void)tid_b.Finish(&arr_tid);
    (void)ts_b.Finish(&arr_ts);
    (void)seq_b.Finish(&arr_seq);
    (void)tick_b.Finish(&arr_tick);
    (void)hash_b.Finish(&arr_hash);
    (void)bp_list.Finish(&arr_bp);
    (void)bs_list.Finish(&arr_bs);
    (void)ap_list.Finish(&arr_ap);
    (void)as_list.Finish(&arr_as);
    (void)trade_price_b.Finish(&arr_tp);
    (void)trade_size_b.Finish(&arr_tsz);
    (void)trade_side_b.Finish(&arr_ts2);
    (void)fee_b.Finish(&arr_fee);
    (void)trade_ts_b.Finish(&arr_tts);
    (void)has_trade_b.Finish(&arr_ht);

    auto table = arrow::Table::Make(schema,
        {arr_cid, arr_tid, arr_ts, arr_seq, arr_tick, arr_hash,
         arr_bp, arr_bs, arr_ap, arr_as,
         arr_tp, arr_tsz, arr_ts2, arr_fee, arr_tts, arr_ht});

    std::string path = snapshot_path(book.get_asset().token_id());
    std::string parent = parent_path(path);
    if (!parent.empty()) {
        (void)fs_->CreateDir(parent, /*recursive=*/true);
    }

    auto outfile = fs_->OpenOutputStream(path).ValueOrDie();
    (void)::parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile, 1);
    (void)outfile->Close();
}

std::optional<OrderBook> ParquetOrderBookRepository::get_latest_snapshot(
    const MarketAsset& asset) const {
    std::lock_guard lock(mutex_);

    std::string path = snapshot_path(asset.token_id());
    auto file_info = fs_->GetFileInfo(path);
    if (!file_info.ok() || file_info->type() == arrow::fs::FileType::NotFound) {
        return std::nullopt;
    }

    auto infile = fs_->OpenInputFile(path).ValueOrDie();
    std::unique_ptr<::parquet::arrow::FileReader> reader;
    auto reader_result = ::parquet::arrow::FileReader::Make(arrow::default_memory_pool(), ::parquet::ParquetFileReader::Open(infile));
    if (!reader_result.ok()) return std::nullopt;
    reader = std::move(reader_result).ValueOrDie();

    std::shared_ptr<arrow::Table> table;
    auto read_status = reader->ReadTable(&table);
    if (!read_status.ok() || table->num_rows() == 0) return std::nullopt;

    // Read the single row
    auto cid = std::static_pointer_cast<arrow::StringArray>(
        table->column(0)->chunk(0))->GetString(0);
    auto tid = std::static_pointer_cast<arrow::StringArray>(
        table->column(1)->chunk(0))->GetString(0);

    // Verify this is the right asset
    if (tid != asset.token_id() || cid != asset.condition_id()) return std::nullopt;

    auto ts = std::static_pointer_cast<arrow::Int64Array>(
        table->column(2)->chunk(0))->Value(0);
    auto seq = std::static_pointer_cast<arrow::UInt64Array>(
        table->column(3)->chunk(0))->Value(0);

    // Build a BookSnapshot to apply to an empty book
    MarketAsset snap_asset(cid, tid);
    std::string snap_hash = std::static_pointer_cast<arrow::StringArray>(
        table->column(5)->chunk(0))->GetString(0);

    std::vector<PriceLevel> bids, asks;

    auto bp_list = std::static_pointer_cast<arrow::ListArray>(table->column(6)->chunk(0));
    auto bs_list = std::static_pointer_cast<arrow::ListArray>(table->column(7)->chunk(0));
    auto bp_values = std::static_pointer_cast<arrow::DoubleArray>(bp_list->values());
    auto bs_values = std::static_pointer_cast<arrow::DoubleArray>(bs_list->values());

    int32_t bp_start = bp_list->value_offset(0);
    int32_t bp_end = bp_list->value_offset(1);
    for (int32_t j = bp_start; j < bp_end; ++j) {
        bids.emplace_back(Price(bp_values->Value(j)), Quantity(bs_values->Value(j)));
    }

    auto ap_list = std::static_pointer_cast<arrow::ListArray>(table->column(8)->chunk(0));
    auto as_list = std::static_pointer_cast<arrow::ListArray>(table->column(9)->chunk(0));
    auto ap_values = std::static_pointer_cast<arrow::DoubleArray>(ap_list->values());
    auto as_values = std::static_pointer_cast<arrow::DoubleArray>(as_list->values());

    int32_t ap_start = ap_list->value_offset(0);
    int32_t ap_end = ap_list->value_offset(1);
    for (int32_t j = ap_start; j < ap_end; ++j) {
        asks.emplace_back(Price(ap_values->Value(j)), Quantity(as_values->Value(j)));
    }

    BookSnapshot snap{{snap_asset, Timestamp(ts), seq}, std::move(bids), std::move(asks), snap_hash};
    auto book = OrderBook::empty(snap_asset).apply(snap);

    // Apply tick size if different from default
    auto tick_size = std::static_pointer_cast<arrow::DoubleArray>(
        table->column(4)->chunk(0))->Value(0);
    if (tick_size != 0.01) {
        TickSizeChange tick_change{{snap_asset, Timestamp(ts), seq}, Price(0.01), Price(tick_size)};
        book = book.apply(tick_change);
    }

    // Apply trade if present
    auto has_trade = std::static_pointer_cast<arrow::BooleanArray>(
        table->column(15)->chunk(0))->Value(0);
    if (has_trade) {
        TradeEvent trade{
            {snap_asset,
             Timestamp(std::static_pointer_cast<arrow::Int64Array>(
                 table->column(14)->chunk(0))->Value(0)),
             seq},
            Price(std::static_pointer_cast<arrow::DoubleArray>(
                table->column(10)->chunk(0))->Value(0)),
            Quantity(std::static_pointer_cast<arrow::DoubleArray>(
                table->column(11)->chunk(0))->Value(0)),
            static_cast<Side>(std::static_pointer_cast<arrow::UInt8Array>(
                table->column(12)->chunk(0))->Value(0)),
            std::static_pointer_cast<arrow::StringArray>(
                table->column(13)->chunk(0))->GetString(0)
        };
        book = book.apply(trade);
    }

    return book;
}

// --- Path helpers ---

std::string ParquetOrderBookRepository::events_dir(
    const std::string& event_type, const std::string& token_id) const {
    return "events/" + event_type + "/" + token_prefix(token_id);
}

std::string ParquetOrderBookRepository::snapshot_path(const std::string& token_id) const {
    return "snapshots/" + token_hash(token_id) + ".parquet";
}

std::string ParquetOrderBookRepository::token_prefix(const std::string& token_id) {
    return token_id.substr(0, std::min<size_t>(8, token_id.size()));
}

std::string ParquetOrderBookRepository::token_hash(const std::string& token_id) {
    return token_id.substr(0, std::min<size_t>(16, token_id.size()));
}

std::string ParquetOrderBookRepository::date_string(int64_t timestamp_ms) {
    auto seconds = timestamp_ms / 1000;
    std::time_t time = static_cast<std::time_t>(seconds);
    std::tm tm{};
    gmtime_r(&time, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

std::string ParquetOrderBookRepository::hour_string(int64_t timestamp_ms) {
    auto seconds = timestamp_ms / 1000;
    std::time_t time = static_cast<std::time_t>(seconds);
    std::tm tm{};
    gmtime_r(&time, &tm);
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << tm.tm_hour;
    return oss.str();
}

} // namespace mde::repositories::pq
