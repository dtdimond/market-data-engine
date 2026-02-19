#include "repositories/parquet/ParquetSchemas.hpp"

#include <gtest/gtest.h>

using namespace mde::repositories::pq;

TEST(ParquetSchemas, BookSnapshotSchemaHasCorrectFields) {
    auto schema = ParquetSchemas::book_snapshot_schema();
    ASSERT_EQ(schema->num_fields(), 9);

    EXPECT_EQ(schema->field(0)->name(), "condition_id");
    EXPECT_EQ(schema->field(1)->name(), "token_id");
    EXPECT_EQ(schema->field(2)->name(), "timestamp_ms");
    EXPECT_EQ(schema->field(3)->name(), "sequence_number");
    EXPECT_EQ(schema->field(4)->name(), "hash");
    EXPECT_EQ(schema->field(5)->name(), "bid_prices");
    EXPECT_EQ(schema->field(6)->name(), "bid_sizes");
    EXPECT_EQ(schema->field(7)->name(), "ask_prices");
    EXPECT_EQ(schema->field(8)->name(), "ask_sizes");

    EXPECT_TRUE(schema->field(0)->type()->Equals(arrow::utf8()));
    EXPECT_TRUE(schema->field(2)->type()->Equals(arrow::int64()));
    EXPECT_TRUE(schema->field(3)->type()->Equals(arrow::uint64()));
    EXPECT_TRUE(schema->field(5)->type()->Equals(arrow::list(arrow::float64())));
}

TEST(ParquetSchemas, BookDeltaSchemaHasCorrectFields) {
    auto schema = ParquetSchemas::book_delta_schema();
    ASSERT_EQ(schema->num_fields(), 10);

    EXPECT_EQ(schema->field(4)->name(), "change_asset_ids");
    EXPECT_EQ(schema->field(5)->name(), "change_prices");
    EXPECT_EQ(schema->field(6)->name(), "change_new_sizes");
    EXPECT_EQ(schema->field(7)->name(), "change_sides");
    EXPECT_EQ(schema->field(8)->name(), "change_best_bids");
    EXPECT_EQ(schema->field(9)->name(), "change_best_asks");

    EXPECT_TRUE(schema->field(4)->type()->Equals(arrow::list(arrow::utf8())));
    EXPECT_TRUE(schema->field(7)->type()->Equals(arrow::list(arrow::uint8())));
}

TEST(ParquetSchemas, TradeEventSchemaHasCorrectFields) {
    auto schema = ParquetSchemas::trade_event_schema();
    ASSERT_EQ(schema->num_fields(), 8);

    EXPECT_EQ(schema->field(4)->name(), "price");
    EXPECT_EQ(schema->field(5)->name(), "size");
    EXPECT_EQ(schema->field(6)->name(), "side");
    EXPECT_EQ(schema->field(7)->name(), "fee_rate_bps");

    EXPECT_TRUE(schema->field(4)->type()->Equals(arrow::float64()));
    EXPECT_TRUE(schema->field(6)->type()->Equals(arrow::uint8()));
}

TEST(ParquetSchemas, TickSizeChangeSchemaHasCorrectFields) {
    auto schema = ParquetSchemas::tick_size_change_schema();
    ASSERT_EQ(schema->num_fields(), 6);

    EXPECT_EQ(schema->field(4)->name(), "old_tick_size");
    EXPECT_EQ(schema->field(5)->name(), "new_tick_size");

    EXPECT_TRUE(schema->field(4)->type()->Equals(arrow::float64()));
}

TEST(ParquetSchemas, OrderBookSnapshotSchemaHasCorrectFields) {
    auto schema = ParquetSchemas::order_book_snapshot_schema();
    ASSERT_EQ(schema->num_fields(), 16);

    EXPECT_EQ(schema->field(4)->name(), "tick_size");
    EXPECT_EQ(schema->field(5)->name(), "book_hash");
    EXPECT_EQ(schema->field(10)->name(), "trade_price");
    EXPECT_EQ(schema->field(15)->name(), "has_trade");

    EXPECT_TRUE(schema->field(15)->type()->Equals(arrow::boolean()));
}

TEST(ParquetSchemas, AllSchemasShareBaseColumns) {
    auto schemas = {
        ParquetSchemas::book_snapshot_schema(),
        ParquetSchemas::book_delta_schema(),
        ParquetSchemas::trade_event_schema(),
        ParquetSchemas::tick_size_change_schema(),
    };

    for (const auto& schema : schemas) {
        EXPECT_EQ(schema->field(0)->name(), "condition_id");
        EXPECT_EQ(schema->field(1)->name(), "token_id");
        EXPECT_EQ(schema->field(2)->name(), "timestamp_ms");
        EXPECT_EQ(schema->field(3)->name(), "sequence_number");

        EXPECT_TRUE(schema->field(0)->type()->Equals(arrow::utf8()));
        EXPECT_TRUE(schema->field(1)->type()->Equals(arrow::utf8()));
        EXPECT_TRUE(schema->field(2)->type()->Equals(arrow::int64()));
        EXPECT_TRUE(schema->field(3)->type()->Equals(arrow::uint64()));
    }
}
