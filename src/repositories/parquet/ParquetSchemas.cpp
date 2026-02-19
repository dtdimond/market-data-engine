#include "repositories/parquet/ParquetSchemas.hpp"

namespace mde::repositories::pq {

namespace {

arrow::FieldVector base_event_fields() {
    return {
        arrow::field("condition_id", arrow::utf8()),
        arrow::field("token_id", arrow::utf8()),
        arrow::field("timestamp_ms", arrow::int64()),
        arrow::field("sequence_number", arrow::uint64()),
    };
}

arrow::FieldVector extend(arrow::FieldVector base, arrow::FieldVector extra) {
    base.insert(base.end(), extra.begin(), extra.end());
    return base;
}

} // namespace

std::shared_ptr<arrow::Schema> ParquetSchemas::book_snapshot_schema() {
    return arrow::schema(extend(base_event_fields(), {
        arrow::field("hash", arrow::utf8()),
        arrow::field("bid_prices", arrow::list(arrow::float64())),
        arrow::field("bid_sizes", arrow::list(arrow::float64())),
        arrow::field("ask_prices", arrow::list(arrow::float64())),
        arrow::field("ask_sizes", arrow::list(arrow::float64())),
    }));
}

std::shared_ptr<arrow::Schema> ParquetSchemas::book_delta_schema() {
    return arrow::schema(extend(base_event_fields(), {
        arrow::field("change_asset_ids", arrow::list(arrow::utf8())),
        arrow::field("change_prices", arrow::list(arrow::float64())),
        arrow::field("change_new_sizes", arrow::list(arrow::float64())),
        arrow::field("change_sides", arrow::list(arrow::uint8())),
        arrow::field("change_best_bids", arrow::list(arrow::float64())),
        arrow::field("change_best_asks", arrow::list(arrow::float64())),
    }));
}

std::shared_ptr<arrow::Schema> ParquetSchemas::trade_event_schema() {
    return arrow::schema(extend(base_event_fields(), {
        arrow::field("price", arrow::float64()),
        arrow::field("size", arrow::float64()),
        arrow::field("side", arrow::uint8()),
        arrow::field("fee_rate_bps", arrow::utf8()),
    }));
}

std::shared_ptr<arrow::Schema> ParquetSchemas::tick_size_change_schema() {
    return arrow::schema(extend(base_event_fields(), {
        arrow::field("old_tick_size", arrow::float64()),
        arrow::field("new_tick_size", arrow::float64()),
    }));
}

std::shared_ptr<arrow::Schema> ParquetSchemas::order_book_snapshot_schema() {
    return arrow::schema({
        arrow::field("condition_id", arrow::utf8()),
        arrow::field("token_id", arrow::utf8()),
        arrow::field("timestamp_ms", arrow::int64()),
        arrow::field("sequence_number", arrow::uint64()),
        arrow::field("tick_size", arrow::float64()),
        arrow::field("book_hash", arrow::utf8()),
        arrow::field("bid_prices", arrow::list(arrow::float64())),
        arrow::field("bid_sizes", arrow::list(arrow::float64())),
        arrow::field("ask_prices", arrow::list(arrow::float64())),
        arrow::field("ask_sizes", arrow::list(arrow::float64())),
        arrow::field("trade_price", arrow::float64()),
        arrow::field("trade_size", arrow::float64()),
        arrow::field("trade_side", arrow::uint8()),
        arrow::field("trade_fee_rate_bps", arrow::utf8()),
        arrow::field("trade_timestamp_ms", arrow::int64()),
        arrow::field("has_trade", arrow::boolean()),
    });
}

} // namespace mde::repositories::pq
