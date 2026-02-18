#include "infrastructure/PolymarketClient.hpp"

namespace mde::infrastructure {

PolymarketClient::PolymarketClient(const mde::config::WebSocketSettings& settings) {
    ws_.setUrl(settings.url);
    ws_.setPingInterval(settings.ping_interval_seconds);

    ws_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        on_message(msg);
    });
}

PolymarketClient::~PolymarketClient() {
    ws_.stop();
}

void PolymarketClient::set_on_event(EventCallback callback) {
    std::lock_guard lock(callback_mutex_);
    on_event_ = std::move(callback);
}

void PolymarketClient::subscribe(const std::string& token_id) {
    token_ids_.push_back(token_id);
    if (connected_) {
        send_subscribe();
    }
}

void PolymarketClient::start() {
    ws_.start();
}

void PolymarketClient::stop() {
    ws_.stop();
}

void PolymarketClient::on_message(const ix::WebSocketMessagePtr& msg) {
    switch (msg->type) {
        case ix::WebSocketMessageType::Open:
            connected_ = true;
            if (!token_ids_.empty()) {
                send_subscribe();
            }
            break;

        case ix::WebSocketMessageType::Message: {
            auto events = parser_.parse(msg->str);
            std::lock_guard lock(callback_mutex_);
            if (on_event_) {
                for (const auto& event : events) {
                    on_event_(event);
                }
            }
            break;
        }

        case ix::WebSocketMessageType::Close:
            connected_ = false;
            break;

        default:
            break;
    }
}

void PolymarketClient::send_subscribe() {
    std::string assets = "[";
    for (size_t i = 0; i < token_ids_.size(); ++i) {
        if (i > 0) assets += ",";
        assets += "\"" + token_ids_[i] + "\"";
    }
    assets += "]";
    ws_.send(R"({"assets_ids": )" + assets + R"(, "type": "market"})");
}

} // namespace mde::infrastructure
