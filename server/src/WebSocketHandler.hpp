#pragma once
#include "User.hpp"
#include "nlohmann/json.hpp"
#include <libwebsockets.h>
#include <list>
#include <string>
using json = nlohmann::json; // Define a shorthand for the json type

class WebSocketHandler {
public:
    static int startServer(int port);
    static int callbackEcho(struct lws* connection,
        enum lws_callback_reasons reason, void* user,
        void* in, size_t len);
    // Handles one message from a connected client. Throws on invalid input.
    static void handleEvent(User& sender, const std::string& message);

    // Thread-safe: queues a message for every connected user in the room and
    // wakes the service thread, which does the actual writes. libwebsockets
    // only supports writes from its own service thread.
    static void queueBroadcast(const std::string& room_id, const std::string& message);
};
