# Market Data Engine - Architecture

## Overview

A high-performance C++ market data processing engine using **Hexagonal Architecture** and **Domain-Driven Design** principles. Built for real-time order book tracking on Polymarket prediction markets via the CLOB WebSocket API.

## Core Principles

- **Hexagonal Architecture**: Core domain logic isolated from external dependencies
- **Domain-Driven Design**: Rich domain models, single bounded context around OrderBook
- **Dependency Inversion**: Core depends on nothing; adapters depend on core
- **Immutability**: Domain objects are immutable for thread-safety and clarity
- **Event Sourcing**: Store the events that build the OrderBook; derive current state as a projection
- **Testability**: In-memory repositories for unit tests, real adapters for integration

---

## Data Source: Polymarket CLOB WebSocket

The engine consumes real-time market data from the Polymarket CLOB WebSocket Market Channel.

**Connection:** `wss://ws-subscriptions-clob.polymarket.com/ws/market`
**Auth:** None required for the market channel (only the user channel needs API keys).
**Subscribe:** `{"assets_ids": ["<token_id>", ...], "type": "market"}`
**Limit:** Max 500 assets per WebSocket connection.
**Keep-alive:** Ping/pong required.

### Polymarket Concepts

- **Market**: A prediction question (e.g., "Will X happen?"), identified by a `condition_id` (hex string).
- **Asset**: One side of a binary outcome (Yes or No), identified by a `token_id` (large integer string). Each market has exactly two assets.
- **Price**: A probability between 0.00 and 1.00, represented as a string.
- **Size**: Quantity of shares at a price level, represented as a string (can be fractional).

### Message Types (Market Channel)

The CLOB WebSocket emits four message types on the market channel. The first three are the core events that drive the OrderBook:

**1. `book` — Full order book snapshot**

Emitted on first subscribe and whenever a trade affects the book. This is a complete replacement of the book state.

```json
{
  "event_type": "book",
  "asset_id": "6581861...",
  "market": "0xbd31dc...",
  "bids": [
    {"price": "0.48", "size": "30"},
    {"price": "0.49", "size": "20"}
  ],
  "asks": [
    {"price": "0.52", "size": "25"},
    {"price": "0.53", "size": "60"}
  ],
  "timestamp": "123456789000",
  "hash": "0x0..."
}
```

**2. `price_change` — Incremental book delta**

Emitted when an order is placed or cancelled (book changes *without* a trade). Gives the new aggregate size at affected price levels, plus the updated best bid/ask.

```json
{
  "event_type": "price_change",
  "market": "0x5f651...",
  "timestamp": "1757908892351",
  "price_changes": [
    {
      "asset_id": "71321...",
      "price": "0.5",
      "size": "200",
      "side": "BUY",
      "hash": "56621a...",
      "best_bid": "0.5",
      "best_ask": "1"
    }
  ]
}
```

**3. `last_trade_price` — Trade execution**

Emitted when a maker and taker order match.

```json
{
  "event_type": "last_trade_price",
  "asset_id": "11412207...",
  "market": "0x6a67b...",
  "price": "0.456",
  "side": "BUY",
  "size": "219.217767",
  "fee_rate_bps": "0",
  "timestamp": "1750428146322"
}
```

**4. `tick_size_change` — Tick size change**

Emitted when price approaches extremes (>0.96 or <0.04), changing the minimum price increment.

```json
{
  "event_type": "tick_size_change",
  "asset_id": "6581861...",
  "market": "0xbd31dc...",
  "old_tick_size": "0.01",
  "new_tick_size": "0.001",
  "timestamp": "100000000"
}
```

### Event Sequencing

Note that `book` and `last_trade_price` are both triggered by trades: a trade match produces a `last_trade_price` and a new `book` snapshot. Between trades, order placements/cancellations produce only `price_change` deltas. To maintain an accurate live book, both `book` (full snapshots) and `price_change` (deltas) must be processed.

---

## Architecture Layers

### Layer 1: Domain (Core)

The heart of the application. Contains business logic, value objects, events, and the OrderBook aggregate. **Zero external dependencies.**

#### Bounded Context: Order Book

The **OrderBook** is the single aggregate. Everything else — prices, trades, price levels — exists as a sub-component that feeds into or is derived from the book.

**Value Objects (Immutable):**

```cpp
// domain/value_objects/

// Probability price between 0.00 and 1.00
class Price {
  double value;  // 0.0 to 1.0
};

// Share quantity at a price level (can be fractional)
class Quantity {
  double size;
};

class Timestamp {
  int64_t milliseconds_since_epoch;  // Polymarket uses millisecond precision
};

// Identifies a market + asset (one side of a binary outcome)
class MarketAsset {
  string condition_id;  // Market ID (hex string, e.g., "0xbd31dc...")
  string token_id;      // Asset ID (large integer string)
};

// A single price level in the book
struct PriceLevel {
  Price price;
  Quantity size;
};
```

**Events (Immutable, append-only):**

These map directly to Polymarket CLOB WebSocket messages. They are the raw facts that build the OrderBook over time and serve as the source of truth in the event store.

```cpp
// domain/events/

struct OrderBookEvent {
  MarketAsset asset;
  Timestamp timestamp;
  uint64_t sequence_number;  // Locally assigned, monotonic ordering
};

// From "book" message — full snapshot replacement
// Triggered by: first subscribe, trade execution
struct BookSnapshot : OrderBookEvent {
  vector<PriceLevel> bids;
  vector<PriceLevel> asks;
  string hash;  // Polymarket's hash of book content
};

// From "price_change" message — incremental delta
// Triggered by: order placement, order cancellation
struct BookDelta : OrderBookEvent {
  vector<PriceLevelDelta> changes;
};

struct PriceLevelDelta {
  string asset_id;
  Price price;
  Quantity new_size;  // New aggregate size at this level (0 = level removed)
  Side side;          // BUY or SELL
  Price best_bid;     // Updated BBO after this change
  Price best_ask;
};

// From "last_trade_price" message
// Triggered by: maker/taker order match
struct TradeEvent : OrderBookEvent {
  Price price;
  Quantity size;
  Side side;  // BUY or SELL (aggressor side)
  string fee_rate_bps;
};

// From "tick_size_change" message
// Triggered by: price approaching extremes (>0.96 or <0.04)
struct TickSizeChange : OrderBookEvent {
  Price old_tick_size;
  Price new_tick_size;
};
```

**Aggregate: OrderBook**

The OrderBook is the single aggregate root. It is **built from events** and can be queried for current state. The book itself is an immutable snapshot — applying an event produces a new book.

```cpp
// domain/aggregates/
class OrderBook {
  MarketAsset asset;
  vector<PriceLevel> bids;
  vector<PriceLevel> asks;
  optional<TradeEvent> latest_trade;
  Price tick_size;
  Timestamp timestamp;
  uint64_t last_sequence_number;
  string book_hash;

public:
  // Apply events to produce a new OrderBook (immutable — returns new instance)
  // BookSnapshot: replaces entire book state
  // BookDelta: patches individual price levels
  // TradeEvent: records latest trade (book snapshot follows separately)
  // TickSizeChange: updates tick size
  OrderBook apply(const OrderBookEvent& event) const;

  // Query current state
  Spread get_spread() const;
  int get_depth() const;
  Price get_midpoint() const;
  Price get_best_bid() const;
  Price get_best_ask() const;
  optional<TradeEvent> get_latest_trade() const;

  // Factory
  static OrderBook empty(MarketAsset asset);
};
```

---

### Layer 2: Repository Interface (Port)

**Defined in domain layer, implemented in infrastructure layer.**

The repository has two responsibilities: **event storage** (source of truth) and **snapshot storage** (projection for fast reads). The interface exposes both.

```cpp
// repositories/OrderBookRepository.hpp
class IOrderBookRepository {
public:
  // Event storage (source of truth)
  virtual void append_event(const OrderBookEvent& event) = 0;
  virtual vector<OrderBookEvent> get_events(
    MarketAsset asset, TimeRange range) const = 0;
  virtual vector<OrderBookEvent> get_events_since(
    MarketAsset asset, uint64_t sequence_number) const = 0;

  // Snapshot storage (projection for fast reads)
  virtual void store_snapshot(const OrderBook& book) = 0;
  virtual optional<OrderBook> get_latest_snapshot(MarketAsset asset) const = 0;

  virtual ~IOrderBookRepository() = default;
};
```

**Rebuilding state:** To reconstruct an OrderBook at any point in time, load the nearest prior snapshot (or start from `OrderBook::empty`), then replay events forward. This gives you both fast reads and full history.

---

### Layer 3: Service

**Orchestrates event ingestion, projection maintenance, and queries.**

```cpp
// services/OrderBookService.hpp
class OrderBookService {
private:
  IOrderBookRepository& repository;

  // In-memory projection: the current book per asset
  map<MarketAsset, OrderBook> current_books;

  // Snapshot policy
  uint64_t snapshot_interval;  // Snapshot every N events

public:
  OrderBookService(IOrderBookRepository& repo, uint64_t snapshot_interval = 1000);

  // Ingestion: receive events from feed, persist, update projection
  void on_event(const OrderBookEvent& event);

  // Queries against current projection (fast, no DB hit)
  OrderBook get_current_book(MarketAsset asset) const;
  Spread get_current_spread(MarketAsset asset) const;
  Price get_midpoint(MarketAsset asset) const;

  // Historical queries (replays from event store)
  OrderBook reconstruct_at(MarketAsset asset, Timestamp time) const;

private:
  void maybe_snapshot(const MarketAsset& asset, uint64_t sequence_number);
};
```

---

## Project Structure

```
market-data-engine/
├── docs/
│   ├── ARCHITECTURE.md        # This file
│   ├── DEVELOPMENT.md         # Build, test, workflow guide
│   └── ROADMAP.md             # Implementation phases
├── src/
│   ├── domain/
│   │   ├── value_objects/
│   │   │   ├── Price.hpp / Price.cpp
│   │   │   ├── Quantity.hpp / Quantity.cpp
│   │   │   ├── MarketAsset.hpp / MarketAsset.cpp
│   │   │   ├── PriceLevel.hpp / PriceLevel.cpp
│   │   │   └── Timestamp.hpp / Timestamp.cpp
│   │   ├── events/
│   │   │   ├── OrderBookEvent.hpp
│   │   │   ├── BookSnapshot.hpp
│   │   │   ├── BookDelta.hpp
│   │   │   ├── TradeEvent.hpp
│   │   │   └── TickSizeChange.hpp
│   │   └── aggregates/
│   │       └── OrderBook.hpp / OrderBook.cpp
│   ├── repositories/
│   │   └── OrderBookRepository.hpp
│   ├── services/
│   │   ├── OrderBookService.hpp
│   │   └── OrderBookService.cpp
│   └── main.cpp  // Composition root - wires everything together
├── tests/
│   ├── domain/
│   │   ├── value_objects/
│   │   │   └── PriceTest.cpp
│   │   └── aggregates/
│   │       └── OrderBookTest.cpp
│   ├── services/
│   │   └── OrderBookServiceTest.cpp
│   └── integration/
│       └── PolymarketIntegrationTest.cpp
└── CMakeLists.txt
```

---

## Data Flow

### Ingestion Flow

```
Polymarket CLOB WebSocket Message
  ↓
PolymarketMessageParser::parse()
  → BookSnapshot   (from "book" message)
  → BookDelta      (from "price_change" message)
  → TradeEvent     (from "last_trade_price" message)
  → TickSizeChange (from "tick_size_change" message)
  ↓
OrderBookService::on_event(event)
  ↓
  ├─→ repository.append_event(event)           [persist event — source of truth]
  ├─→ current_books[asset].apply(event)        [update in-memory projection]
  └─→ maybe_snapshot(asset, seq)               [periodically persist snapshot]
```

### Query Flow (Current State)

```
Query: "What's the current spread for market 0xbd31dc, Yes token?"
  ↓
OrderBookService::get_current_spread(asset)
  ↓
current_books[asset].get_spread()              [direct read from in-memory projection]
  ↓
return Spread result                           [no DB hit]
```

### Query Flow (Historical Reconstruction)

```
Query: "What did the book look like at timestamp 1750428146322?"
  ↓
OrderBookService::reconstruct_at(asset, target_time)
  ↓
repository.get_latest_snapshot(asset)          [load nearest prior snapshot]
  ↓ (or OrderBook::empty if no snapshot)
repository.get_events_since(asset, snapshot.last_sequence_number)
  ↓
replay events up to target_time                [fold events onto snapshot]
  ↓
return reconstructed OrderBook
```

---

## Event Sourcing Model

### Why Both Events and Snapshots?

**Events** are the source of truth. They capture every change to the order book and are append-only. This gives you full history, auditability, and the ability to reconstruct state at any point in time.

**Snapshots** are a performance optimization. Replaying thousands of events to get current state is expensive. Periodically persisting the current projection means reconstruction only needs to replay events since the last snapshot.

Note: Polymarket's `book` message is itself a full snapshot from the exchange. These are useful for consistency checks (compare our locally-maintained book against the exchange's snapshot), but in the event store they're just another event type — `BookSnapshot` replaces the full book state when applied.

### Snapshot Policy

The `OrderBookService` snapshots based on a configurable interval (e.g., every 1000 events). This is a tunable tradeoff between storage cost and reconstruction speed. Other policies are possible (time-based, on-demand) and can be added later.

### Consistency

The in-memory projection is always the most current state. The event store is the durable source of truth. If the process restarts, the service loads the latest snapshot and replays events from that point forward to rebuild the projection.

The `book` message hash can be used to verify that our locally-maintained book (built from `BookDelta` events) matches the exchange's view after each trade.

---

## Testing Strategy

### Unit Tests (Domain)

**Test the aggregate and value objects in isolation.**

```cpp
// tests/domain/aggregates/OrderBookTest.cpp

TEST(OrderBook, ApplyBookSnapshotReplacesEntireBook) {
  auto asset = MarketAsset("0xbd31dc...", "6581861...");
  auto book = OrderBook::empty(asset);

  BookSnapshot snapshot;
  snapshot.asset = asset;
  snapshot.bids = {{Price(0.48), Quantity(30)}, {Price(0.49), Quantity(20)}};
  snapshot.asks = {{Price(0.52), Quantity(25)}, {Price(0.53), Quantity(60)}};
  snapshot.sequence_number = 1;

  auto updated = book.apply(snapshot);

  EXPECT_EQ(updated.get_depth(), 2);  // 2 levels each side
  EXPECT_DOUBLE_EQ(updated.get_best_bid().value, 0.49);
  EXPECT_DOUBLE_EQ(updated.get_best_ask().value, 0.52);
}

TEST(OrderBook, ApplyBookDeltaPatchesOneLevel) {
  auto asset = MarketAsset("0xbd31dc...", "6581861...");
  auto book = OrderBook::empty(asset);

  // Start with a snapshot
  BookSnapshot snapshot;
  snapshot.asset = asset;
  snapshot.bids = {{Price(0.48), Quantity(30)}};
  snapshot.asks = {{Price(0.52), Quantity(25)}};
  snapshot.sequence_number = 1;
  book = book.apply(snapshot);

  // Apply a delta: new bid at 0.50
  BookDelta delta;
  delta.asset = asset;
  delta.changes = {{
    .asset_id = "6581861...",
    .price = Price(0.50),
    .new_size = Quantity(100),
    .side = Side::BUY,
    .best_bid = Price(0.50),
    .best_ask = Price(0.52)
  }};
  delta.sequence_number = 2;

  auto updated = book.apply(delta);

  EXPECT_EQ(updated.get_depth(), 2);  // Now 2 bid levels
  EXPECT_DOUBLE_EQ(updated.get_best_bid().value, 0.50);
}

TEST(OrderBook, ApplyTradeRecordsLatestTrade) {
  auto asset = MarketAsset("0xbd31dc...", "6581861...");
  auto book = OrderBook::empty(asset);

  TradeEvent trade;
  trade.asset = asset;
  trade.price = Price(0.456);
  trade.size = Quantity(219.22);
  trade.side = Side::BUY;
  trade.sequence_number = 1;

  auto updated = book.apply(trade);

  ASSERT_TRUE(updated.get_latest_trade().has_value());
  EXPECT_DOUBLE_EQ(updated.get_latest_trade()->price.value, 0.456);
}
```

### Service Tests

**Use in-memory repository to test event flow and projection logic.**

```cpp
// tests/services/OrderBookServiceTest.cpp

TEST(OrderBookService, PersistsEventsAndUpdatesProjection) {
  InMemoryOrderBookRepository repo;
  OrderBookService service(repo, /*snapshot_interval=*/100);

  auto asset = MarketAsset("0xbd31dc...", "6581861...");

  BookSnapshot snapshot;
  snapshot.asset = asset;
  snapshot.bids = {{Price(0.48), Quantity(30)}};
  snapshot.asks = {{Price(0.52), Quantity(25)}};
  snapshot.sequence_number = 1;

  service.on_event(snapshot);

  // Event was persisted
  auto events = repo.get_events(asset, TimeRange::all());
  EXPECT_EQ(events.size(), 1);

  // Projection was updated
  auto book = service.get_current_book(asset);
  EXPECT_DOUBLE_EQ(book.get_best_bid().value, 0.48);
}

TEST(OrderBookService, BookHashConsistencyCheck) {
  InMemoryOrderBookRepository repo;
  OrderBookService service(repo, /*snapshot_interval=*/100);

  auto asset = MarketAsset("0xbd31dc...", "6581861...");

  // Apply some deltas, then receive a book snapshot
  // Verify our local book matches the exchange snapshot
  // (implementation detail — but this is the key consistency mechanism)
}
```

---

## Key Design Decisions

### Why a Single Aggregate (OrderBook)?

Trades, price level changes, and tick size updates all describe the same thing: the state of the market for an asset. Collapsing them into a single aggregate simplifies the model, reduces the number of repositories, and makes the event stream coherent — every event is an OrderBook event.

### Why Event Sourcing?

- **Full history**: Reconstruct the book at any point in time for backtesting or debugging
- **Auditability**: Every change is recorded as an immutable event
- **Flexibility**: Derive new projections later without changing the event store
- **Natural fit**: The Polymarket WebSocket is inherently a stream of events

### Why Both BookSnapshot and BookDelta?

These map directly to the two Polymarket message types that mutate the book:

- **`book` → BookSnapshot**: Full replacement. Emitted on subscribe and on every trade. Provides a consistency checkpoint.
- **`price_change` → BookDelta**: Incremental patch. Emitted on order placement/cancellation (book changes without a trade). Required to keep the book accurate between trades.

Without BookDelta, the local book would be stale whenever a limit order is placed or cancelled — only updating on the next trade.

### Why Immutability?

- **Thread-safety**: Immutable objects can be safely shared across threads without locks
- **Clarity**: No hidden mutations, easier to reason about
- **Event sourcing alignment**: `apply(event)` returns a new book, never mutates

### Why Hexagonal Architecture?

- **Testability**: Core logic completely isolated from I/O
- **Flexibility**: Swap Polymarket for another exchange by changing the message parser and composition root
- **Future-proof**: Easy to add new adapters (new exchanges, new storage backends)

---

## Next Steps

See `ROADMAP.md` for detailed implementation phases.
See `DEVELOPMENT.md` for build instructions and workflow.
