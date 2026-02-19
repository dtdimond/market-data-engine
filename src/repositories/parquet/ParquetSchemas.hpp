#pragma once

#include <arrow/api.h>

namespace mde::repositories::pq {

class ParquetSchemas {
public:
    // Event schemas
    static std::shared_ptr<arrow::Schema> book_snapshot_schema();
    static std::shared_ptr<arrow::Schema> book_delta_schema();
    static std::shared_ptr<arrow::Schema> trade_event_schema();
    static std::shared_ptr<arrow::Schema> tick_size_change_schema();

    // Snapshot file schema (for OrderBook persistence)
    static std::shared_ptr<arrow::Schema> order_book_snapshot_schema();
};

} // namespace mde::repositories::pq
