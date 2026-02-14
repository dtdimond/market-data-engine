#pragma once

#include <compare>
#include <cstdint>
#include <string>

namespace mde::domain {

class Timestamp {
public:
    explicit Timestamp(int64_t milliseconds_since_epoch);

    static Timestamp from_string(const std::string& str);

    int64_t milliseconds() const noexcept { return ms_; }

    bool operator==(const Timestamp&) const = default;
    auto operator<=>(const Timestamp&) const = default;

private:
    int64_t ms_;
};

} // namespace mde::domain
