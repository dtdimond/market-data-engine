#pragma once

#include "infrastructure/PolymarketMessageParser.hpp"
#include "services/IMarketDataFeed.hpp"

#include <ixwebsocket/IXWebSocket.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace mde::infrastructure {

class PolymarketClient : public mde::services::IMarketDataFeed {
public:
    PolymarketClient();
    ~PolymarketClient() override;

    void set_on_event(EventCallback callback) override;
    void subscribe(const std::string& token_id) override;
    void start() override;
    void stop() override;

private:
    ix::WebSocket ws_;
    PolymarketMessageParser parser_;
    EventCallback on_event_;
    std::vector<std::string> token_ids_;
    std::atomic<bool> connected_{false};
    std::mutex callback_mutex_;

    void on_message(const ix::WebSocketMessagePtr& msg);
    void send_subscribe();
};

} // namespace mde::infrastructure
