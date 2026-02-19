#include "repositories/InMemoryOrderBookRepository.hpp"
#include "services/IMarketDataFeed.hpp"
#include "services/OrderBookService.hpp"

#include <gtest/gtest.h>

using namespace mde::domain;
using namespace mde::services;
using mde::repositories::InMemoryOrderBookRepository;

// --- Test fake ---

class FakeMarketDataFeed : public IMarketDataFeed {
    EventCallback on_event_;

public:
    void set_on_event(EventCallback cb) override { on_event_ = std::move(cb); }
    void subscribe(const std::string&) override {}
    void start() override {}
    void stop() override {}

    void emit(const OrderBookEventVariant& event) {
        if (on_event_) on_event_(event);
    }
};

// --- Fixture ---

class OrderBookServiceTest : public ::testing::Test {
protected:
    InMemoryOrderBookRepository repo;
    FakeMarketDataFeed feed;

    MarketAsset asset{"0xbd31dc", "6581861"};

    BookSnapshot make_snapshot() {
        return BookSnapshot{
            {asset, Timestamp(1000), 0},
            {PriceLevel(Price(0.48), Quantity(30.0)), PriceLevel(Price(0.49), Quantity(20.0))},
            {PriceLevel(Price(0.52), Quantity(25.0)), PriceLevel(Price(0.53), Quantity(60.0))},
            "0xabc"
        };
    }
};

// --- Event ingestion ---

TEST_F(OrderBookServiceTest, PersistsEventAndUpdatesProjection) {
    OrderBookService service(repo, feed);

    feed.emit(make_snapshot());

    EXPECT_EQ(repo.event_count(), 1);

    auto& book = service.get_current_book(asset);
    EXPECT_EQ(book.get_depth(), 2);
    EXPECT_DOUBLE_EQ(book.get_best_bid().value(), 0.49);
    EXPECT_DOUBLE_EQ(book.get_best_ask().value(), 0.52);
}

TEST_F(OrderBookServiceTest, AssignsSequenceNumbers) {
    OrderBookService service(repo, feed);

    feed.emit(make_snapshot());

    TradeEvent trade{
        {asset, Timestamp(2000), 0},
        Price(0.50), Quantity(10.0), Side::BUY, "0"
    };
    feed.emit(trade);

    auto& events = repo.events();
    auto seq1 = std::visit([](const auto& e) { return e.sequence_number; }, events[0]);
    auto seq2 = std::visit([](const auto& e) { return e.sequence_number; }, events[1]);

    EXPECT_EQ(seq1, 1);
    EXPECT_EQ(seq2, 2);
}

TEST_F(OrderBookServiceTest, AppliesMultipleEventsInSequence) {
    OrderBookService service(repo, feed);

    feed.emit(make_snapshot());

    BookDelta delta{
        {asset, Timestamp(2000), 0},
        {PriceLevelDelta{"6581861", Price(0.50), Quantity(100.0), Side::BUY,
                         Price(0.50), Price(0.52)}}
    };
    feed.emit(delta);

    auto& book = service.get_current_book(asset);
    EXPECT_EQ(book.get_depth(), 3);
    EXPECT_DOUBLE_EQ(book.get_best_bid().value(), 0.50);
}

TEST_F(OrderBookServiceTest, TracksLatestTrade) {
    OrderBookService service(repo, feed);

    feed.emit(make_snapshot());

    TradeEvent trade{
        {asset, Timestamp(2000), 0},
        Price(0.50), Quantity(10.0), Side::BUY, "0"
    };
    feed.emit(trade);

    auto& book = service.get_current_book(asset);
    ASSERT_TRUE(book.get_latest_trade().has_value());
    EXPECT_DOUBLE_EQ(book.get_latest_trade()->price.value(), 0.50);
}

// --- Queries ---

TEST_F(OrderBookServiceTest, GetCurrentSpread) {
    OrderBookService service(repo, feed);
    feed.emit(make_snapshot());

    auto spread = service.get_current_spread(asset);
    EXPECT_DOUBLE_EQ(spread.best_bid.value(), 0.49);
    EXPECT_DOUBLE_EQ(spread.best_ask.value(), 0.52);
    EXPECT_NEAR(spread.value(), 0.03, 1e-10);
}

TEST_F(OrderBookServiceTest, GetMidpoint) {
    OrderBookService service(repo, feed);
    feed.emit(make_snapshot());

    auto mid = service.get_midpoint(asset);
    EXPECT_DOUBLE_EQ(mid.value(), 0.505);
}

TEST_F(OrderBookServiceTest, ThrowsOnUnknownAsset) {
    OrderBookService service(repo, feed);
    MarketAsset unknown("0x000", "999");
    EXPECT_THROW(service.get_current_book(unknown), std::runtime_error);
}

// --- Snapshot policy ---

TEST_F(OrderBookServiceTest, SnapshotsAtConfiguredInterval) {
    OrderBookService service(repo, feed, /*snapshot_interval=*/3);

    feed.emit(make_snapshot());  // seq 1
    EXPECT_FALSE(repo.has_snapshot(asset));

    TradeEvent trade{
        {asset, Timestamp(2000), 0},
        Price(0.50), Quantity(10.0), Side::BUY, "0"
    };
    feed.emit(trade);  // seq 2
    EXPECT_FALSE(repo.has_snapshot(asset));

    BookDelta delta{
        {asset, Timestamp(3000), 0},
        {PriceLevelDelta{"6581861", Price(0.50), Quantity(100.0), Side::BUY,
                         Price(0.50), Price(0.52)}}
    };
    feed.emit(delta);  // seq 3
    EXPECT_TRUE(repo.has_snapshot(asset));
}

// --- resolve_asset and event_count ---

TEST_F(OrderBookServiceTest, ResolveAssetFindsKnownToken) {
    OrderBookService service(repo, feed);
    feed.emit(make_snapshot());

    auto resolved = service.resolve_asset("6581861");
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->condition_id(), "0xbd31dc");
    EXPECT_EQ(resolved->token_id(), "6581861");
}

TEST_F(OrderBookServiceTest, ResolveAssetReturnsNulloptForUnknown) {
    OrderBookService service(repo, feed);
    feed.emit(make_snapshot());

    auto resolved = service.resolve_asset("unknown_token");
    EXPECT_FALSE(resolved.has_value());
}

TEST_F(OrderBookServiceTest, EventCountStartsAtZero) {
    OrderBookService service(repo, feed);
    EXPECT_EQ(service.event_count(), 0);
}

TEST_F(OrderBookServiceTest, EventCountIncrementsWithEvents) {
    OrderBookService service(repo, feed);
    feed.emit(make_snapshot());
    EXPECT_EQ(service.event_count(), 1);

    TradeEvent trade{
        {asset, Timestamp(2000), 0},
        Price(0.50), Quantity(10.0), Side::BUY, "0"
    };
    feed.emit(trade);
    EXPECT_EQ(service.event_count(), 2);
}

TEST_F(OrderBookServiceTest, NoSnapshotWhenIntervalZero) {
    OrderBookService service(repo, feed, /*snapshot_interval=*/0);

    feed.emit(make_snapshot());
    feed.emit(make_snapshot());
    feed.emit(make_snapshot());

    EXPECT_FALSE(repo.has_snapshot(asset));
}
