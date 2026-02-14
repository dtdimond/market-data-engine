#include "domain/value_objects/MarketAsset.hpp"

#include <stdexcept>

namespace mde::domain {

MarketAsset::MarketAsset(std::string condition_id, std::string token_id)
    : condition_id_(std::move(condition_id))
    , token_id_(std::move(token_id)) {
    if (condition_id_.empty()) {
        throw std::invalid_argument("MarketAsset condition_id must not be empty");
    }
    if (token_id_.empty()) {
        throw std::invalid_argument("MarketAsset token_id must not be empty");
    }
}

} // namespace mde::domain
