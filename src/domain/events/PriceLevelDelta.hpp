#pragma once

#include "domain/value_objects/Price.hpp"
#include "domain/value_objects/Quantity.hpp"
#include "domain/value_objects/Side.hpp"

#include <string>

namespace mde::domain {

struct PriceLevelDelta {
    std::string asset_id;
    Price price;
    Quantity new_size;
    Side side;
    Price best_bid;
    Price best_ask;
};

} // namespace mde::domain
