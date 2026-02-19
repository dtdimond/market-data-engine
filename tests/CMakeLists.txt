add_executable(market_data_engine_tests
    config/SettingsTest.cpp
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
    services/OrderBookServiceTest.cpp
)

target_link_libraries(market_data_engine_tests PRIVATE
    config
    domain
    infrastructure
    services
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(market_data_engine_tests)

# Parquet unit tests (schema validation, serialization roundtrips — no disk I/O)
if(MDE_HAS_PARQUET)
    add_executable(parquet_unit_tests
        infrastructure/parquet/ParquetSchemasTest.cpp
        infrastructure/parquet/ParquetSerializationTest.cpp
    )

    target_link_libraries(parquet_unit_tests PRIVATE
        parquet_repository
        domain
        GTest::gtest_main
    )

    gtest_discover_tests(parquet_unit_tests DISCOVERY_TIMEOUT 30)
endif()

# Parquet integration tests (disk I/O, opt-in)
if(MDE_HAS_PARQUET AND MDE_INTEGRATION_TESTS)
    add_executable(parquet_integration_tests
        infrastructure/parquet/ParquetIntegrationTest.cpp
    )

    target_link_libraries(parquet_integration_tests PRIVATE
        parquet_repository
        domain
        config
        GTest::gtest_main
    )

    gtest_discover_tests(parquet_integration_tests DISCOVERY_TIMEOUT 30)
endif()
