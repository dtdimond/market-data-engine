#pragma once

#include "config/Settings.hpp"

#include <arrow/filesystem/filesystem.h>

#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace mde::infrastructure {

class MarketDiscovery {
public:
    MarketDiscovery(std::shared_ptr<arrow::fs::FileSystem> fs,
                    const config::ApiSettings& api,
                    const config::DiscoverySettings& discovery);
    virtual ~MarketDiscovery() = default;

    void load();
    std::vector<std::string> tracked_token_ids() const;
    size_t tracked_count() const;
    bool at_capacity() const;

    // Poll Gamma API, add new markets, persist, fire callback with new IDs.
    // Returns number of newly added markets.
    size_t poll(std::function<void(const std::vector<std::string>&)> on_new);

protected:
    virtual std::vector<std::string> fetch_top_token_ids(int limit) const;

private:
    void persist() const;

    std::shared_ptr<arrow::fs::FileSystem> fs_;
    config::ApiSettings api_;
    config::DiscoverySettings discovery_;
    std::set<std::string> tracked_ids_;
    mutable std::mutex mutex_;

    static constexpr const char* kTrackedFile = "tracked_markets.json";
};

} // namespace mde::infrastructure
