#include "infrastructure/MarketDiscovery.hpp"

#include <arrow/filesystem/api.h>
#include <arrow/filesystem/mockfs.h>
#include <arrow/io/api.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace mde::infrastructure;
using namespace mde::config;

namespace {

// Test subclass that overrides fetch_top_token_ids to avoid real HTTP calls
class FakeDiscovery : public MarketDiscovery {
public:
    using MarketDiscovery::MarketDiscovery;

    void set_fake_ids(std::vector<std::string> ids) {
        fake_ids_ = std::move(ids);
    }

protected:
    std::vector<std::string> fetch_top_token_ids(int limit) const override {
        if (static_cast<int>(fake_ids_.size()) <= limit) return fake_ids_;
        return {fake_ids_.begin(), fake_ids_.begin() + limit};
    }

private:
    std::vector<std::string> fake_ids_;
};

std::shared_ptr<arrow::fs::FileSystem> make_mock_fs() {
    auto mock = std::make_shared<arrow::fs::internal::MockFileSystem>(
        arrow::fs::TimePoint(std::chrono::seconds(0)));
    return std::make_shared<arrow::fs::SubTreeFileSystem>("/", mock);
}

} // namespace

TEST(MarketDiscovery, LoadFromEmptyFilesystem) {
    auto fs = make_mock_fs();
    ApiSettings api;
    DiscoverySettings disc;

    FakeDiscovery discovery(fs, api, disc);
    EXPECT_NO_THROW(discovery.load());
    EXPECT_EQ(discovery.tracked_count(), 0u);
}

TEST(MarketDiscovery, LoadFromPrePopulatedJson) {
    auto fs = make_mock_fs();

    // Write a tracked_markets.json file
    std::string json = R"({"tracked_token_ids": ["token_a", "token_b", "token_c"]})";
    auto result = fs->OpenOutputStream("tracked_markets.json");
    ASSERT_TRUE(result.ok());
    auto stream = *result;
    ASSERT_TRUE(stream->Write(json.data(), static_cast<int64_t>(json.size())).ok());
    ASSERT_TRUE(stream->Close().ok());

    ApiSettings api;
    DiscoverySettings disc;

    FakeDiscovery discovery(fs, api, disc);
    discovery.load();
    EXPECT_EQ(discovery.tracked_count(), 3u);

    auto ids = discovery.tracked_token_ids();
    EXPECT_NE(std::find(ids.begin(), ids.end(), "token_a"), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), "token_b"), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), "token_c"), ids.end());
}

TEST(MarketDiscovery, PersistAndLoadRoundTrip) {
    auto fs = make_mock_fs();
    ApiSettings api;
    DiscoverySettings disc;

    // Create discovery, poll to add IDs, which triggers persist
    FakeDiscovery d1(fs, api, disc);
    d1.set_fake_ids({"id_1", "id_2"});
    size_t added = d1.poll(nullptr);
    EXPECT_EQ(added, 2u);

    // Create new discovery instance and load from same fs
    FakeDiscovery d2(fs, api, disc);
    d2.load();
    EXPECT_EQ(d2.tracked_count(), 2u);

    auto ids = d2.tracked_token_ids();
    EXPECT_NE(std::find(ids.begin(), ids.end(), "id_1"), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), "id_2"), ids.end());
}

TEST(MarketDiscovery, PollAddsOnlyNewIds) {
    auto fs = make_mock_fs();
    ApiSettings api;
    DiscoverySettings disc;

    FakeDiscovery discovery(fs, api, disc);

    // First poll
    discovery.set_fake_ids({"a", "b", "c"});
    size_t added1 = discovery.poll(nullptr);
    EXPECT_EQ(added1, 3u);
    EXPECT_EQ(discovery.tracked_count(), 3u);

    // Second poll with overlapping + new IDs
    discovery.set_fake_ids({"b", "c", "d"});
    size_t added2 = discovery.poll(nullptr);
    EXPECT_EQ(added2, 1u);  // only "d" is new
    EXPECT_EQ(discovery.tracked_count(), 4u);
}

TEST(MarketDiscovery, PollFiresCallbackWithOnlyNewIds) {
    auto fs = make_mock_fs();
    ApiSettings api;
    DiscoverySettings disc;

    FakeDiscovery discovery(fs, api, disc);

    // Seed with existing IDs
    discovery.set_fake_ids({"x", "y"});
    discovery.poll(nullptr);

    // Poll again with mix of old and new
    discovery.set_fake_ids({"x", "y", "z"});
    std::vector<std::string> callback_ids;
    discovery.poll([&](const std::vector<std::string>& new_ids) {
        callback_ids = new_ids;
    });

    ASSERT_EQ(callback_ids.size(), 1u);
    EXPECT_EQ(callback_ids[0], "z");
}

TEST(MarketDiscovery, PollRespectsCapacityCap) {
    auto fs = make_mock_fs();
    ApiSettings api;
    DiscoverySettings disc;
    disc.max_tracked_markets = 3;

    FakeDiscovery discovery(fs, api, disc);
    discovery.set_fake_ids({"a", "b", "c", "d", "e"});

    size_t added = discovery.poll(nullptr);
    EXPECT_EQ(added, 3u);
    EXPECT_TRUE(discovery.at_capacity());
}

TEST(MarketDiscovery, PollDoesNothingWhenAtCapacity) {
    auto fs = make_mock_fs();
    ApiSettings api;
    DiscoverySettings disc;
    disc.max_tracked_markets = 2;

    FakeDiscovery discovery(fs, api, disc);
    discovery.set_fake_ids({"a", "b"});
    discovery.poll(nullptr);
    EXPECT_TRUE(discovery.at_capacity());

    // Try to add more
    discovery.set_fake_ids({"c", "d"});
    bool callback_called = false;
    size_t added = discovery.poll([&](const std::vector<std::string>&) {
        callback_called = true;
    });
    EXPECT_EQ(added, 0u);
    EXPECT_FALSE(callback_called);
}

TEST(MarketDiscovery, WorksWithNullFilesystem) {
    ApiSettings api;
    DiscoverySettings disc;

    FakeDiscovery discovery(nullptr, api, disc);
    EXPECT_NO_THROW(discovery.load());  // no crash

    discovery.set_fake_ids({"a", "b"});
    size_t added = discovery.poll(nullptr);
    EXPECT_EQ(added, 2u);
    EXPECT_EQ(discovery.tracked_count(), 2u);
}
