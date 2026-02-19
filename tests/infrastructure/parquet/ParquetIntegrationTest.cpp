#include "repositories/parquet/ParquetOrderBookRepository.hpp"

#include "domain/aggregates/OrderBook.hpp"
#include "domain/events/BookDelta.hpp"
#include "domain/events/BookSnapshot.hpp"
#include "domain/events/TickSizeChange.hpp"
#include "domain/events/TradeEvent.hpp"

#include <arrow/filesystem/api.h>
#include <arrow/filesystem/mockfs.h>
#include <gtest/gtest.h>

#include <variant>

using namespace mde::domain;
using namespace mde::repositories::pq;

class ParquetIntegrationTest : public ::testing::Test {
protected:
    std::shared_ptr<arrow::fs::FileSystem> fs_;

    void SetUp() override {
        auto mock_fs = std::make_shared<arrow::fs::internal::MockFileSystem>(
            arrow::fs::TimePoint(std::chrono::seconds(0)));
        fs_ = std::make_shared<arrow::fs::SubTreeFileSystem>("/", mock_fs);
    }

    mde::config::StorageSettings make_settings(int buffer_size = 10) {
        mde::config::StorageSettings s;
        s.write_buffer_size = buffer_size;
        return s;
    }

    MarketAsset asset{"0xbd31dc", "6581861"};

    BookSnapshot make_snapshot(uint64_t seq = 0) {
        return BookSnapshot{
            {asset, Timestamp(1000), seq},
            {PriceLevel(Price(0.48), Quantity(30.0)), PriceLevel(Price(0.49), Quantity(20.0))},
            {PriceLevel(Price(0.52), Quantity(25.0)), PriceLevel(Price(0.53), Quantity(60.0))},
            "0xhash"
        };
    }

    TradeEvent make_trade(uint64_t seq = 0) {
        return TradeEvent{
            {asset, Timestamp(2000), seq},
            Price(0.50), Quantity(10.0), Side::BUY, "100"
        };
    }

    BookDelta make_delta(uint64_t seq = 0) {
        return BookDelta{
            {asset, Timestamp(3000), seq},
            {PriceLevelDelta{"6581861", Price(0.50), Quantity(100.0), Side::BUY,
                             Price(0.50), Price(0.52)}}
        };
    }
};

TEST_F(ParquetIntegrationTest, AppendAndReadEventsRoundtrip) {
    // Use small buffer to force immediate flush
    auto settings = make_settings(3);
    ParquetOrderBookRepository repo(fs_, settings);

    auto snap = make_snapshot(1);
    auto trade = make_trade(2);
    auto delta = make_delta(3);

    repo.append_event(snap);
    repo.append_event(trade);
    repo.append_event(delta);

    // Buffer should have flushed (3 events >= buffer_size of 3)
    auto events = repo.get_events_since(asset, 0);
    ASSERT_EQ(events.size(), 3);

    // Verify sequence ordering
    auto seq1 = std::visit([](const auto& e) { return e.sequence_number; }, events[0]);
    auto seq2 = std::visit([](const auto& e) { return e.sequence_number; }, events[1]);
    auto seq3 = std::visit([](const auto& e) { return e.sequence_number; }, events[2]);
    EXPECT_EQ(seq1, 1);
    EXPECT_EQ(seq2, 2);
    EXPECT_EQ(seq3, 3);
}

TEST_F(ParquetIntegrationTest, UnflushedEventsAreVisibleInRead) {
    // Large buffer so nothing flushes
    auto settings = make_settings(1000);
    ParquetOrderBookRepository repo(fs_, settings);

    repo.append_event(make_snapshot(1));
    repo.append_event(make_trade(2));

    auto events = repo.get_events_since(asset, 0);
    EXPECT_EQ(events.size(), 2);
}

TEST_F(ParquetIntegrationTest, FlushOnDestructor) {
    auto settings = make_settings(1000);

    {
        ParquetOrderBookRepository repo(fs_, settings);
        repo.append_event(make_snapshot(1));
        repo.append_event(make_trade(2));
        // Destructor flushes
    }

    // New repo should read the flushed events
    ParquetOrderBookRepository repo2(fs_, settings);
    auto events = repo2.get_events_since(asset, 0);
    EXPECT_EQ(events.size(), 2);
}

TEST_F(ParquetIntegrationTest, SequenceFilteringWorks) {
    auto settings = make_settings(1);
    ParquetOrderBookRepository repo(fs_, settings);

    repo.append_event(make_snapshot(1));
    repo.append_event(make_trade(2));
    repo.append_event(make_delta(3));

    auto events = repo.get_events_since(asset, 2);
    ASSERT_EQ(events.size(), 1);
    auto seq = std::visit([](const auto& e) { return e.sequence_number; }, events[0]);
    EXPECT_EQ(seq, 3);
}

TEST_F(ParquetIntegrationTest, AssetFilteringWorks) {
    auto settings = make_settings(1);
    ParquetOrderBookRepository repo(fs_, settings);

    repo.append_event(make_snapshot(1));

    MarketAsset other_asset("0xother", "999");
    auto events = repo.get_events_since(other_asset, 0);
    EXPECT_EQ(events.size(), 0);
}

TEST_F(ParquetIntegrationTest, SnapshotStoreAndLoad) {
    auto settings = make_settings(1000);
    ParquetOrderBookRepository repo(fs_, settings);

    auto snap = make_snapshot(5);
    auto book = OrderBook::empty(asset).apply(snap);

    repo.store_snapshot(book);

    auto loaded = repo.get_latest_snapshot(asset);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->get_depth(), 2);
    EXPECT_DOUBLE_EQ(loaded->get_best_bid().value(), 0.49);
    EXPECT_DOUBLE_EQ(loaded->get_best_ask().value(), 0.52);
    EXPECT_EQ(loaded->get_book_hash(), "0xhash");
}

TEST_F(ParquetIntegrationTest, SnapshotWithTradePreservesTrade) {
    auto settings = make_settings(1000);
    ParquetOrderBookRepository repo(fs_, settings);

    auto snap = make_snapshot(1);
    auto trade = make_trade(2);
    auto book = OrderBook::empty(asset).apply(snap).apply(trade);

    repo.store_snapshot(book);

    auto loaded = repo.get_latest_snapshot(asset);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE(loaded->get_latest_trade().has_value());
    EXPECT_DOUBLE_EQ(loaded->get_latest_trade()->price.value(), 0.50);
}

TEST_F(ParquetIntegrationTest, SnapshotReturnsNulloptForUnknownAsset) {
    auto settings = make_settings(1000);
    ParquetOrderBookRepository repo(fs_, settings);

    MarketAsset unknown("0x000", "999");
    auto loaded = repo.get_latest_snapshot(unknown);
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(ParquetIntegrationTest, BookSnapshotEventDataPreserved) {
    auto settings = make_settings(1);
    ParquetOrderBookRepository repo(fs_, settings);

    auto snap = make_snapshot(1);
    repo.append_event(snap);

    auto events = repo.get_events_since(asset, 0);
    ASSERT_EQ(events.size(), 1);

    auto& read_snap = std::get<BookSnapshot>(events[0]);
    EXPECT_EQ(read_snap.bids.size(), 2);
    EXPECT_EQ(read_snap.asks.size(), 2);
    EXPECT_DOUBLE_EQ(read_snap.bids[0].price().value(), 0.48);
    EXPECT_DOUBLE_EQ(read_snap.asks[0].price().value(), 0.52);
    EXPECT_EQ(read_snap.hash, "0xhash");
}

TEST_F(ParquetIntegrationTest, TradeEventDataPreserved) {
    auto settings = make_settings(1);
    ParquetOrderBookRepository repo(fs_, settings);

    auto trade = make_trade(1);
    repo.append_event(trade);

    auto events = repo.get_events_since(asset, 0);
    ASSERT_EQ(events.size(), 1);

    auto& read_trade = std::get<TradeEvent>(events[0]);
    EXPECT_DOUBLE_EQ(read_trade.price.value(), 0.50);
    EXPECT_DOUBLE_EQ(read_trade.size.size(), 10.0);
    EXPECT_EQ(read_trade.side, Side::BUY);
    EXPECT_EQ(read_trade.fee_rate_bps, "100");
}

TEST_F(ParquetIntegrationTest, BookDeltaEventDataPreserved) {
    auto settings = make_settings(1);
    ParquetOrderBookRepository repo(fs_, settings);

    auto delta = make_delta(1);
    repo.append_event(delta);

    auto events = repo.get_events_since(asset, 0);
    ASSERT_EQ(events.size(), 1);

    auto& read_delta = std::get<BookDelta>(events[0]);
    ASSERT_EQ(read_delta.changes.size(), 1);
    EXPECT_EQ(read_delta.changes[0].asset_id, "6581861");
    EXPECT_DOUBLE_EQ(read_delta.changes[0].price.value(), 0.50);
    EXPECT_DOUBLE_EQ(read_delta.changes[0].new_size.size(), 100.0);
    EXPECT_EQ(read_delta.changes[0].side, Side::BUY);
}
