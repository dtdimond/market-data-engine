#pragma once

#include <string>

namespace mde::domain {

class MarketAsset {
public:
    MarketAsset(std::string condition_id, std::string token_id);

    const std::string& condition_id() const noexcept { return condition_id_; }
    const std::string& token_id() const noexcept { return token_id_; }

    bool operator==(const MarketAsset&) const = default;
    auto operator<=>(const MarketAsset&) const = default;

private:
    std::string condition_id_;
    std::string token_id_;
};

} // namespace mde::domain
