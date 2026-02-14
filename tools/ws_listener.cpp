#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

static std::atomic<bool> running{true};

void signal_handler(int) {
    running = false;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ws_listener <token_id> [token_id2 ...]" << std::endl;
        return 1;
    }

    ix::initNetSystem();

    // Build assets array from CLI args
    std::string assets = "[";
    for (int i = 1; i < argc; ++i) {
        if (i > 1) assets += ",";
        assets += "\"" + std::string(argv[i]) + "\"";
    }
    assets += "]";

    ix::WebSocket ws;
    ws.setUrl("wss://ws-subscriptions-clob.polymarket.com/ws/market");
    ws.setPingInterval(30);

    ws.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        switch (msg->type) {
            case ix::WebSocketMessageType::Open:
                std::cout << "[connected] Subscribing to " << argc - 1
                          << " asset(s)..." << std::endl;
                ws.send(R"({"assets_ids": )" + assets + R"(, "type": "market"})");
                break;

            case ix::WebSocketMessageType::Message:
                std::cout << msg->str << "\n" << std::endl;
                break;

            case ix::WebSocketMessageType::Error:
                std::cerr << "[error] " << msg->errorInfo.reason << std::endl;
                break;

            case ix::WebSocketMessageType::Close:
                std::cout << "[disconnected]" << std::endl;
                break;

            default:
                break;
        }
    });

    std::signal(SIGINT, signal_handler);

    ws.start();
    std::cout << "Listening... (Ctrl+C to quit)" << std::endl;

    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ws.stop();
    ix::uninitNetSystem();
    std::cout << "\nDone." << std::endl;
    return 0;
}
