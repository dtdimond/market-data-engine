add_executable(market_data_engine_tests
    domain/value_objects/PriceTest.cpp
    domain/value_objects/QuantityTest.cpp
    domain/value_objects/TimestampTest.cpp
)

target_link_libraries(market_data_engine_tests PRIVATE
    domain
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(market_data_engine_tests)
