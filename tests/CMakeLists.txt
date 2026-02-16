add_executable(market_data_engine_tests
    domain/value_objects/PriceTest.cpp
    domain/value_objects/QuantityTest.cpp
    domain/value_objects/TimestampTest.cpp
    domain/value_objects/MarketAssetTest.cpp
    domain/value_objects/PriceLevelTest.cpp
    domain/events/SideTest.cpp
    domain/events/BookSnapshotTest.cpp
    domain/events/BookDeltaTest.cpp
    domain/events/TradeEventTest.cpp
    domain/events/TickSizeChangeTest.cpp
    domain/aggregates/OrderBookTest.cpp
    infrastructure/PolymarketMessageParserTest.cpp
)

target_link_libraries(market_data_engine_tests PRIVATE
    domain
    infrastructure
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(market_data_engine_tests)
