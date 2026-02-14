# CLAUDE.md

## Project

Market data engine for Polymarket prediction markets. Ingests real-time order book data via WebSocket, stores events, and maintains live projections. C++20 with CMake. Google Test for testing.

## Architecture

Read `docs/ARCHITECTURE.md` before making any structural decisions. It is the source of truth for:

- Project structure and layer boundaries
- The OrderBook aggregate and event sourcing model
- Repository interface design
- Event types and data flow

Key principles:
- **Hexagonal architecture**: domain core has zero external dependencies
- **Single aggregate**: OrderBook is the only aggregate root. Trades, quotes, price levels are sub-components
- **Event sourcing**: events are the source of truth, snapshots are performance projections
- **Immutability**: domain objects are immutable. `OrderBook::apply(event)` returns a new book, never mutates
- **Dependency inversion**: repository interfaces defined in domain, implemented elsewhere

## Project Structure

```
src/
├── domain/
│   ├── value_objects/    # Price, Quantity, Symbol, Timestamp — immutable, value semantics
│   ├── entities/         # Trade, Quote, OptionContract — immutable snapshots
│   ├── events/           # OrderBookEvent hierarchy — append-only source of truth
│   └── aggregates/       # OrderBook — single aggregate root
├── repositories/         # IOrderBookRepository interface (port)
├── services/             # OrderBookService — orchestrates events, projections, queries
└── main.cpp              # Composition root
tests/
├── domain/
├── services/
└── integration/
```

## Data Source: Polymarket CLOB WebSocket

Connection: `wss://ws-subscriptions-clob.polymarket.com/ws/market`
No authentication required for market channel. Max 500 assets per connection. Ping/pong required.

Subscribe by sending: `{"assets_ids": ["<token_id>"], "type": "market"}`

### Message Types

**`book`** — Full order book snapshot. Emitted on first subscribe and on every trade.
```json
{
  "event_type": "book",
  "asset_id": "<token_id>",
  "market": "<condition_id>",
  "bids": [{"price": "0.48", "size": "30"}],
  "asks": [{"price": "0.52", "size": "25"}],
  "timestamp": "123456789000",
  "hash": "0x..."
}
```

**`price_change`** — Incremental level delta. Emitted on order placement or cancellation (no trade).
```json
{
  "event_type": "price_change",
  "market": "<condition_id>",
  "timestamp": "...",
  "price_changes": [{
    "asset_id": "<token_id>",
    "price": "0.5",
    "size": "200",
    "side": "BUY",
    "hash": "...",
    "best_bid": "0.5",
    "best_ask": "1"
  }]
}
```

**`last_trade_price`** — Trade execution. Emitted when maker/taker orders match.
```json
{
  "event_type": "last_trade_price",
  "asset_id": "<token_id>",
  "market": "<condition_id>",
  "price": "0.456",
  "side": "BUY",
  "size": "219.217767",
  "fee_rate_bps": "0",
  "timestamp": "..."
}
```

**`tick_size_change`** — Tick size changes when price > 0.96 or < 0.04.

### Polymarket Domain Notes

- Prices are probabilities (0 to 1), encoded as strings
- Each market is a binary outcome (Yes/No) with exactly two asset IDs
- A "symbol" maps to a `(market_condition_id, asset_id)` pair, not a ticker
- Sizes are string-encoded decimals

## Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Conventions

- Prefer value types and const references
- No raw `new`/`delete` — use value semantics or smart pointers
- Header + implementation split (`.hpp` / `.cpp`)
- Tests mirror `src/` structure under `tests/`
- Test names: `TEST(ClassName, DescribesExpectedBehavior)`
