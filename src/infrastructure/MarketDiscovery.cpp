#include "infrastructure/MarketDiscovery.hpp"

#include <arrow/io/interfaces.h>
#include <arrow/buffer.h>
#include <ixwebsocket/IXHttpClient.h>
#include <nlohmann/json.hpp>

namespace mde::infrastructure {

MarketDiscovery::MarketDiscovery(std::shared_ptr<arrow::fs::FileSystem> fs,
                                 const config::ApiSettings& api,
                                 const config::DiscoverySettings& discovery)
    : fs_(std::move(fs)), api_(api), discovery_(discovery) {}

void MarketDiscovery::load() {
    if (!fs_) return;

    auto result = fs_->OpenInputFile(kTrackedFile);
    if (!result.ok()) return;  // file doesn't exist yet

    auto file = *result;
    auto size_result = file->GetSize();
    if (!size_result.ok()) return;

    auto buf_result = file->Read(*size_result);
    if (!buf_result.ok()) return;

    std::string content(reinterpret_cast<const char*>((*buf_result)->data()),
                        static_cast<size_t>((*buf_result)->size()));

    auto json = nlohmann::json::parse(content, nullptr, false);
    if (json.is_discarded() || !json.contains("tracked_token_ids")) return;

    std::lock_guard lock(mutex_);
    for (const auto& id : json["tracked_token_ids"]) {
        if (id.is_string()) {
            tracked_ids_.insert(id.get<std::string>());
        }
    }
}

std::vector<std::string> MarketDiscovery::tracked_token_ids() const {
    std::lock_guard lock(mutex_);
    return {tracked_ids_.begin(), tracked_ids_.end()};
}

size_t MarketDiscovery::tracked_count() const {
    std::lock_guard lock(mutex_);
    return tracked_ids_.size();
}

bool MarketDiscovery::at_capacity() const {
    std::lock_guard lock(mutex_);
    return static_cast<int>(tracked_ids_.size()) >= discovery_.max_tracked_markets;
}

size_t MarketDiscovery::poll(std::function<void(const std::vector<std::string>&)> on_new) {
    if (at_capacity()) return 0;

    auto top_ids = fetch_top_token_ids(discovery_.markets_per_poll);

    std::vector<std::string> new_ids;
    {
        std::lock_guard lock(mutex_);
        int remaining = discovery_.max_tracked_markets - static_cast<int>(tracked_ids_.size());

        for (const auto& id : top_ids) {
            if (remaining <= 0) break;
            if (tracked_ids_.insert(id).second) {
                new_ids.push_back(id);
                --remaining;
            }
        }
    }

    if (!new_ids.empty()) {
        persist();
        if (on_new) {
            on_new(new_ids);
        }
    }

    return new_ids.size();
}

std::vector<std::string> MarketDiscovery::fetch_top_token_ids(int limit) const {
    ix::HttpClient client;
    auto args = client.createRequest();
    args->connectTimeout = 10;
    args->transferTimeout = 30;

    std::string url = api_.gamma_api_base_url +
        "/markets?active=true&closed=false&limit=" + std::to_string(limit) +
        "&order=volume24hr&ascending=false";

    auto response = client.get(url, args);
    if (response->statusCode != 200) return {};

    auto json = nlohmann::json::parse(response->body, nullptr, false);
    if (!json.is_array()) return {};

    std::vector<std::string> ids;
    for (const auto& market : json) {
        if (!market.contains("clobTokenIds")) continue;

        // clobTokenIds is a string-encoded JSON array like "[\"id1\",\"id2\"]"
        std::string clob_str = market["clobTokenIds"].get<std::string>();
        auto clob = nlohmann::json::parse(clob_str, nullptr, false);
        if (clob.is_array() && !clob.empty()) {
            // Take [0] — YES side
            ids.push_back(clob[0].get<std::string>());
        }
    }

    return ids;
}

void MarketDiscovery::persist() const {
    if (!fs_) return;

    nlohmann::json json;
    {
        std::lock_guard lock(mutex_);
        json["tracked_token_ids"] = nlohmann::json::array();
        for (const auto& id : tracked_ids_) {
            json["tracked_token_ids"].push_back(id);
        }
    }

    std::string content = json.dump(2);

    auto result = fs_->OpenOutputStream(kTrackedFile);
    if (!result.ok()) return;

    auto stream = *result;
    (void)stream->Write(content.data(), static_cast<int64_t>(content.size()));
    (void)stream->Close();
}

} // namespace mde::infrastructure
