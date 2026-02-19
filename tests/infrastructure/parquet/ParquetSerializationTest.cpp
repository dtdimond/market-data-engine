#include "repositories/parquet/ParquetSchemas.hpp"

#include "domain/aggregates/OrderBook.hpp"
#include "domain/events/BookDelta.hpp"
#include "domain/events/BookSnapshot.hpp"
#include "domain/events/TickSizeChange.hpp"
#include "domain/events/TradeEvent.hpp"

#include <arrow/api.h>
#include <arrow/io/memory.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>

#include <gtest/gtest.h>

using namespace mde::domain;
using namespace mde::repositories::pq;

// Helper: write table to in-memory buffer, read it back
static std::shared_ptr<arrow::Table> roundtrip(const std::shared_ptr<arrow::Table>& table) {
    auto sink = arrow::io::BufferOutputStream::Create().ValueOrDie();
    auto status = ::parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), sink, table->num_rows());
    EXPECT_TRUE(status.ok()) << status.ToString();

    auto buffer = sink->Finish().ValueOrDie();
    auto reader_file = std::make_shared<arrow::io::BufferReader>(buffer);

    auto reader = ::parquet::arrow::FileReader::Make(
        arrow::default_memory_pool(),
        ::parquet::ParquetFileReader::Open(reader_file)).ValueOrDie();

    std::shared_ptr<arrow::Table> result;
    EXPECT_TRUE(reader->ReadTable(&result).ok());
    return result;
}

TEST(ParquetSerialization, BookSnapshotRoundtrip) {
    auto schema = ParquetSchemas::book_snapshot_schema();

    arrow::StringBuilder cid_b, tid_b, hash_b;
    arrow::Int64Builder ts_b;
    arrow::UInt64Builder seq_b;

    auto bp_inner = std::make_shared<arrow::DoubleBuilder>();
    auto bs_inner = std::make_shared<arrow::DoubleBuilder>();
    auto ap_inner = std::make_shared<arrow::DoubleBuilder>();
    auto as_inner = std::make_shared<arrow::DoubleBuilder>();
    arrow::ListBuilder bp_list(arrow::default_memory_pool(), bp_inner);
    arrow::ListBuilder bs_list(arrow::default_memory_pool(), bs_inner);
    arrow::ListBuilder ap_list(arrow::default_memory_pool(), ap_inner);
    arrow::ListBuilder as_list(arrow::default_memory_pool(), as_inner);

    (void)cid_b.Append("0xabc");
    (void)tid_b.Append("12345");
    (void)ts_b.Append(1000);
    (void)seq_b.Append(1);
    (void)hash_b.Append("0xhash");

    (void)bp_list.Append();
    (void)bp_inner->Append(0.48);
    (void)bp_inner->Append(0.49);
    (void)bs_list.Append();
    (void)bs_inner->Append(30.0);
    (void)bs_inner->Append(20.0);

    (void)ap_list.Append();
    (void)ap_inner->Append(0.52);
    (void)as_list.Append();
    (void)as_inner->Append(25.0);

    std::shared_ptr<arrow::Array> a1, a2, a3, a4, a5, a6, a7, a8, a9;
    (void)cid_b.Finish(&a1);
    (void)tid_b.Finish(&a2);
    (void)ts_b.Finish(&a3);
    (void)seq_b.Finish(&a4);
    (void)hash_b.Finish(&a5);
    (void)bp_list.Finish(&a6);
    (void)bs_list.Finish(&a7);
    (void)ap_list.Finish(&a8);
    (void)as_list.Finish(&a9);

    auto table = arrow::Table::Make(schema, {a1, a2, a3, a4, a5, a6, a7, a8, a9});
    auto result = roundtrip(table);

    ASSERT_EQ(result->num_rows(), 1);
    EXPECT_EQ(std::static_pointer_cast<arrow::StringArray>(
        result->column(0)->chunk(0))->GetString(0), "0xabc");
    EXPECT_EQ(std::static_pointer_cast<arrow::StringArray>(
        result->column(1)->chunk(0))->GetString(0), "12345");
    EXPECT_EQ(std::static_pointer_cast<arrow::Int64Array>(
        result->column(2)->chunk(0))->Value(0), 1000);

    auto bp = std::static_pointer_cast<arrow::ListArray>(result->column(5)->chunk(0));
    auto bp_vals = std::static_pointer_cast<arrow::DoubleArray>(bp->values());
    EXPECT_EQ(bp->value_offset(1) - bp->value_offset(0), 2);
    EXPECT_DOUBLE_EQ(bp_vals->Value(0), 0.48);
    EXPECT_DOUBLE_EQ(bp_vals->Value(1), 0.49);
}

TEST(ParquetSerialization, TradeEventRoundtrip) {
    auto schema = ParquetSchemas::trade_event_schema();

    arrow::StringBuilder cid_b, tid_b, fee_b;
    arrow::Int64Builder ts_b;
    arrow::UInt64Builder seq_b;
    arrow::DoubleBuilder price_b, size_b;
    arrow::UInt8Builder side_b;

    (void)cid_b.Append("0xabc");
    (void)tid_b.Append("12345");
    (void)ts_b.Append(2000);
    (void)seq_b.Append(5);
    (void)price_b.Append(0.456);
    (void)size_b.Append(219.22);
    (void)side_b.Append(static_cast<uint8_t>(Side::BUY));
    (void)fee_b.Append("100");

    std::shared_ptr<arrow::Array> a1, a2, a3, a4, a5, a6, a7, a8;
    (void)cid_b.Finish(&a1);
    (void)tid_b.Finish(&a2);
    (void)ts_b.Finish(&a3);
    (void)seq_b.Finish(&a4);
    (void)price_b.Finish(&a5);
    (void)size_b.Finish(&a6);
    (void)side_b.Finish(&a7);
    (void)fee_b.Finish(&a8);

    auto table = arrow::Table::Make(schema, {a1, a2, a3, a4, a5, a6, a7, a8});
    auto result = roundtrip(table);

    ASSERT_EQ(result->num_rows(), 1);
    EXPECT_DOUBLE_EQ(std::static_pointer_cast<arrow::DoubleArray>(
        result->column(4)->chunk(0))->Value(0), 0.456);
    EXPECT_DOUBLE_EQ(std::static_pointer_cast<arrow::DoubleArray>(
        result->column(5)->chunk(0))->Value(0), 219.22);
    EXPECT_EQ(std::static_pointer_cast<arrow::UInt8Array>(
        result->column(6)->chunk(0))->Value(0), static_cast<uint8_t>(Side::BUY));
    EXPECT_EQ(std::static_pointer_cast<arrow::StringArray>(
        result->column(7)->chunk(0))->GetString(0), "100");
}

TEST(ParquetSerialization, BookDeltaRoundtrip) {
    auto schema = ParquetSchemas::book_delta_schema();

    arrow::StringBuilder cid_b, tid_b;
    arrow::Int64Builder ts_b;
    arrow::UInt64Builder seq_b;

    auto aids_inner = std::make_shared<arrow::StringBuilder>();
    auto prices_inner = std::make_shared<arrow::DoubleBuilder>();
    auto sizes_inner = std::make_shared<arrow::DoubleBuilder>();
    auto sides_inner = std::make_shared<arrow::UInt8Builder>();
    auto bbids_inner = std::make_shared<arrow::DoubleBuilder>();
    auto basks_inner = std::make_shared<arrow::DoubleBuilder>();

    arrow::ListBuilder aids_list(arrow::default_memory_pool(), aids_inner);
    arrow::ListBuilder prices_list(arrow::default_memory_pool(), prices_inner);
    arrow::ListBuilder sizes_list(arrow::default_memory_pool(), sizes_inner);
    arrow::ListBuilder sides_list(arrow::default_memory_pool(), sides_inner);
    arrow::ListBuilder bbids_list(arrow::default_memory_pool(), bbids_inner);
    arrow::ListBuilder basks_list(arrow::default_memory_pool(), basks_inner);

    (void)cid_b.Append("0xabc");
    (void)tid_b.Append("12345");
    (void)ts_b.Append(3000);
    (void)seq_b.Append(10);

    (void)aids_list.Append();
    (void)prices_list.Append();
    (void)sizes_list.Append();
    (void)sides_list.Append();
    (void)bbids_list.Append();
    (void)basks_list.Append();

    (void)aids_inner->Append("12345");
    (void)prices_inner->Append(0.50);
    (void)sizes_inner->Append(100.0);
    (void)sides_inner->Append(static_cast<uint8_t>(Side::BUY));
    (void)bbids_inner->Append(0.50);
    (void)basks_inner->Append(0.52);

    std::shared_ptr<arrow::Array> a1, a2, a3, a4, a5, a6, a7, a8, a9, a10;
    (void)cid_b.Finish(&a1);
    (void)tid_b.Finish(&a2);
    (void)ts_b.Finish(&a3);
    (void)seq_b.Finish(&a4);
    (void)aids_list.Finish(&a5);
    (void)prices_list.Finish(&a6);
    (void)sizes_list.Finish(&a7);
    (void)sides_list.Finish(&a8);
    (void)bbids_list.Finish(&a9);
    (void)basks_list.Finish(&a10);

    auto table = arrow::Table::Make(schema, {a1, a2, a3, a4, a5, a6, a7, a8, a9, a10});
    auto result = roundtrip(table);

    ASSERT_EQ(result->num_rows(), 1);
    auto aids = std::static_pointer_cast<arrow::ListArray>(result->column(4)->chunk(0));
    auto aids_vals = std::static_pointer_cast<arrow::StringArray>(aids->values());
    EXPECT_EQ(aids_vals->GetString(0), "12345");
}

TEST(ParquetSerialization, TickSizeChangeRoundtrip) {
    auto schema = ParquetSchemas::tick_size_change_schema();

    arrow::StringBuilder cid_b, tid_b;
    arrow::Int64Builder ts_b;
    arrow::UInt64Builder seq_b;
    arrow::DoubleBuilder old_b, new_b;

    (void)cid_b.Append("0xabc");
    (void)tid_b.Append("12345");
    (void)ts_b.Append(4000);
    (void)seq_b.Append(20);
    (void)old_b.Append(0.01);
    (void)new_b.Append(0.001);

    std::shared_ptr<arrow::Array> a1, a2, a3, a4, a5, a6;
    (void)cid_b.Finish(&a1);
    (void)tid_b.Finish(&a2);
    (void)ts_b.Finish(&a3);
    (void)seq_b.Finish(&a4);
    (void)old_b.Finish(&a5);
    (void)new_b.Finish(&a6);

    auto table = arrow::Table::Make(schema, {a1, a2, a3, a4, a5, a6});
    auto result = roundtrip(table);

    ASSERT_EQ(result->num_rows(), 1);
    EXPECT_DOUBLE_EQ(std::static_pointer_cast<arrow::DoubleArray>(
        result->column(4)->chunk(0))->Value(0), 0.01);
    EXPECT_DOUBLE_EQ(std::static_pointer_cast<arrow::DoubleArray>(
        result->column(5)->chunk(0))->Value(0), 0.001);
}
