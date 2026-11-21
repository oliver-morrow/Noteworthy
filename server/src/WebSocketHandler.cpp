#include "WebSocketHandler.hpp"

#include "CanvasObject.hpp"
#include "Config.hpp"
#include "SendableObject.hpp"
#include "ServerState.hpp"
#include "Stroke.hpp"
#include "User.hpp"
#include "nlohmann/json.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <libwebsockets.h>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

using json = nlohmann::json; // Define a shorthand for the json type

namespace {

std::atomic<lws_context*> g_context { nullptr };

// Messages queued by other threads (HTTP handlers) for the service thread.
std::mutex g_outbox_mutex;
std::vector<std::pair<std::string, std::string>> g_outbox; // room_id, message

void sendToConnectedUsers(RoomState& room, const std::string& message, const User* except = nullptr)
{
    room.forEachUser([&](const User& other) {
        if (&other == except || !other.is_connected || other.socket == nullptr) {
            return;
        }
        other.sendEvent(message);
    });
}

void drainOutbox()
{
    std::vector<std::pair<std::string, std::string>> pending;
    {
        std::lock_guard<std::mutex> lock(g_outbox_mutex);
        pending.swap(g_outbox);
    }
    for (auto& [room_id, message] : pending) {
        try {
            state.manipulateRoom(room_id, [&](RoomState& room) {
                sendToConnectedUsers(room, message);
            });
        } catch (const std::exception& e) {
            // room was evicted after the message was queued
        }
    }
}

std::string extract_query_parameter(struct lws* connection, const std::string& query_param)
{
    char url_encoded_string[128] = { 0 };
    char url_decoded_string[128] = { 0 };

    if (lws_get_urlarg_by_name(connection, query_param.c_str(), url_encoded_string, sizeof(url_encoded_string)) == nullptr) {
        throw std::runtime_error("failed to extract " + query_param);
    }

    int decode_err = lws_urldecode(url_decoded_string, url_encoded_string, sizeof(url_decoded_string));
    if (decode_err < 0) {
        throw std::runtime_error("failed to extract " + query_param);
    }

    // The value after the '=' in a query parameter (e.g., username=david) needs to be extracted
    std::string decoded_str(url_decoded_string);
    size_t pos = decoded_str.find('=');
    if (pos != std::string::npos) {
        return decoded_str.substr(pos + 1);
    }
    return decoded_str;
}

// Sets a close reason; the caller must then return -1 from the callback.
int reject_connection(struct lws* connection, const std::string& message)
{
    std::cout << "closing connection: " << message << std::endl;
    lws_close_reason(connection, LWS_CLOSE_STATUS_POLICY_VIOLATION,
        reinterpret_cast<unsigned char*>(const_cast<char*>(message.data())),
        std::min<size_t>(message.size(), 120));
    return -1;
}

std::unique_ptr<CanvasObject> createCanvasObject(EventObjectType object_type)
{
    switch (object_type) {
    case STROKE:
        return std::make_unique<Stroke>();
    case SYMBOL:
        return std::make_unique<Symbol>();
    case SHAPE:
        return std::make_unique<Shape>();
    default:
        throw std::runtime_error("unsupported canvas object type");
    }
}

// Attaches a newly established connection to its room user. Returns an error
// message, or an empty string on success.
std::string attachConnection(struct lws* connection, User** user_ptr_ptr)
{
    std::string username;
    std::string room_id;
    try {
        username = extract_query_parameter(connection, "username=");
        room_id = extract_query_parameter(connection, "room_id=");
    } catch (const std::exception& e) {
        return "missing username and/or room_id param";
    }

    if (!isValidUsername(username)) {
        return "invalid username";
    }
    if (room_id.empty()) {
        return "missing room_id";
    }

    std::string error;
    try {
        state.manipulateRoom(room_id, [&](RoomState& room) {
            if (!room.isUserInRoom(username)) {
                error = "user not in room";
                return;
            }
            if (room.isUserKicked(username)) {
                error = "user was kicked";
                return;
            }
            if (room.isUserConnectedToRoom(username)) {
                error = "user already connected";
                return;
            }

            User* room_user = room.getUserPtr(username);
            room_user->socket = connection;
            room_user->is_connected = true;
            room_user->rate_tokens = -1;

            delete *user_ptr_ptr; // the temporary placeholder user
            *user_ptr_ptr = room_user;

            json event;
            room_user->createConnectEvent(event);
            sendToConnectedUsers(room, event.dump());
        });
    } catch (const std::range_error& e) {
        return "room doesn't exist";
    }
    return error;
}

void detachConnection(User** user_ptr_ptr)
{
    if (user_ptr_ptr == nullptr || *user_ptr_ptr == nullptr) {
        return;
    }
    User* user = *user_ptr_ptr;
    *user_ptr_ptr = nullptr;

    if (user->room_id.empty()) { // temporary placeholder user
        delete user;
        return;
    }

    try {
        state.manipulateRoom(user->room_id, [&](RoomState& room) {
            user->is_connected = false;
            user->socket = nullptr;
            json event;
            user->createDisconnectEvent(event);
            sendToConnectedUsers(room, event.dump());
        });
    } catch (const std::exception& e) {
        std::cerr << "disconnect cleanup failed: " << e.what() << std::endl;
    }
}

} // namespace

void WebSocketHandler::queueBroadcast(const std::string& room_id, const std::string& message)
{
    {
        std::lock_guard<std::mutex> lock(g_outbox_mutex);
        g_outbox.emplace_back(room_id, message);
    }
    if (lws_context* context = g_context.load()) {
        lws_cancel_service(context); // thread-safe; wakes lws_service()
    }
}

int WebSocketHandler::startServer(int port)
{
    lws_set_log_level(LLL_ERR, NULL);

    struct lws_context_creation_info context_info;
    memset(&context_info, 0, sizeof(context_info));

    static struct lws_protocols protocols[] = {
        { "http", lws_callback_http_dummy, 0, 0 },
        // max message size of 10kb; larger messages arrive fragmented and are dropped
        { "echo-protocol", callbackEcho, sizeof(User*), 10000 },
        { NULL, NULL, 0, 0 } // terminator
    };

    context_info.port = port;
    context_info.protocols = protocols;

    lws_context* context = lws_create_context(&context_info);
    if (!context) {
        std::cerr << "Failed to create websocket context!" << std::endl;
        return -1;
    }
    g_context.store(context);

    std::cout << "WebSocket server started on port " << context_info.port << std::endl;

    const auto& config = Config::get();
    auto last_sweep = std::chrono::steady_clock::now();
    while (lws_service(context, 1000) >= 0) {
        drainOutbox();

        auto now = std::chrono::steady_clock::now();
        if (now - last_sweep > std::chrono::seconds(10)) {
            last_sweep = now;
            size_t evicted = state.evictIdleRooms(std::chrono::minutes(config.room_idle_minutes));
            if (evicted > 0) {
                std::cout << "evicted " << evicted << " idle room(s)" << std::endl;
            }
        }
    }

    g_context.store(nullptr);
    lws_context_destroy(context);
    return 0;
}

int WebSocketHandler::callbackEcho(
    struct lws* connection,
    enum lws_callback_reasons reason,
    void* user,
    void* in,
    size_t len)
{
    auto user_ptr_ptr = static_cast<User**>(user);

    // Nothing thrown in here may escape into libwebsockets' C code.
    try {
        switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            *user_ptr_ptr = new User { "", "" }; // placeholder until attached
            std::string error = attachConnection(connection, user_ptr_ptr);
            if (!error.empty()) {
                return reject_connection(connection, error);
            }
            break;
        }

        case LWS_CALLBACK_SERVER_WRITEABLE: {
            if (*user_ptr_ptr == nullptr || (*user_ptr_ptr)->room_id.empty()) {
                return reject_connection(connection, "not allowed to send message");
            }
            break;
        }

        case LWS_CALLBACK_RECEIVE: {
            User* sender = *user_ptr_ptr;
            if (sender == nullptr || sender->room_id.empty()) {
                return reject_connection(connection, "not assigned a user; not allowed to send message");
            }

            // Fragments of oversized messages are dropped, not reassembled.
            if (!lws_is_first_fragment(connection) || !lws_is_final_fragment(connection)) {
                break;
            }

            const auto& config = Config::get();
            if (!sender->allowMessage(config.ws_messages_per_second, config.ws_message_burst)) {
                break;
            }

            std::string message(static_cast<const char*>(in), len);
            try {
                handleEvent(*sender, message);
            } catch (const std::exception& e) {
                std::cerr << "rejected event from " << sender->username << ": " << e.what() << std::endl;
            }
            break;
        }

        case LWS_CALLBACK_CLOSED: {
            detachConnection(user_ptr_ptr);
            break;
        }

        default:
            break;
        }
    } catch (const std::exception& e) {
        std::cerr << "websocket callback error: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "websocket callback error: unknown exception" << std::endl;
        return -1;
    }
    return 0;
}

void WebSocketHandler::handleEvent(User& sender, const std::string& message)
{
    const auto& config = Config::get();
    if (config.debug_log) {
        std::cout << message << std::endl;
    }

    json event = json::parse(message);
    if (!event.is_object()) {
        throw std::runtime_error("event must be a JSON object");
    }

    // Events always apply to the sender's own room, whatever the payload says.
    event["room_id"] = sender.room_id;

    const int object_type = event.at("object_type").get<int>();
    const int event_type = event.at("event_type").get<int>();
    struct lws* socket_to_kick = nullptr;

    state.manipulateRoom(sender.room_id, [&](RoomState& room) {
        const bool is_owner = room.owner_id == sender.username;

        switch (object_type) {
        case ROOM: {
            if (!is_owner) {
                throw std::runtime_error("only the owner can change room settings");
            }
            switch (event_type) {
            case RoomState::RoomEventType::CHANGE_PASSWORD: {
                std::string new_password = event.at("new_password").get<std::string>();
                if (new_password.size() > 64) {
                    throw std::runtime_error("password too long");
                }
                room.password = new_password;
                break;
            }
            case RoomState::RoomEventType::PROMOTE_USER: {
                std::string username = event.at("username").get<std::string>();
                if (!room.isUserInRoom(username)) {
                    throw std::runtime_error("user not in room");
                }
                room.owner_id = username;
                break;
            }
            default:
                throw std::runtime_error("unsupported room event");
            }
            break;
        }

        case PAGE: {
            switch (event_type) {
            case Page::PageEventType::DELETE: {
                room.deletePage(event.at("page_id").get<uint64_t>());
                break;
            }
            case Page::PageEventType::INSERT: {
                if (room.pageCount() >= config.max_pages_per_room) {
                    throw std::runtime_error("page limit reached");
                }
                event["base64_image"] = ""; // only server-side PDF import may attach images
                auto page = std::make_unique<Page>();
                page->fromJson(event);
                if (room.hasPage(page->page_id)) {
                    throw std::runtime_error("page id already exists");
                }
                room.addPageAfter(event.at("previous_page_id").get<uint64_t>(), std::move(page));
                break;
            }
            default:
                throw std::runtime_error("unsupported page event");
            }
            break;
        }

        case USER: {
            // CONNECT/DISCONNECT come from the server only; clients may only kick.
            if (event_type != User::UserEventType::KICK) {
                throw std::runtime_error("unsupported user event");
            }
            if (!is_owner) {
                throw std::runtime_error("only the owner can kick");
            }
            std::string target = event.at("username").get<std::string>();
            if (target == sender.username) {
                throw std::runtime_error("cannot kick yourself");
            }
            room.manipulateUser(target, [&](User& user) {
                user.is_kicked = true;
                socket_to_kick = user.socket;
            });
            break;
        }

        case STROKE:
        case SYMBOL:
        case SHAPE: {
            const uint64_t page_id = event.at("page_id").get<uint64_t>();
            const uint64_t object_id = event.at("object_id").get<uint64_t>();

            switch (event_type) {
            case CanvasObject::CanvasObjectEventType::CREATE:
                room.manipulatePage(page_id, [&](Page& page) {
                    if (page.objectCount() >= config.max_objects_per_page) {
                        throw std::runtime_error("object limit reached");
                    }
                    auto object = createCanvasObject(static_cast<EventObjectType>(object_type));
                    object->fromJson(event);
                    if (auto* stroke = dynamic_cast<Stroke*>(object.get())) {
                        if (stroke->pointCount() > config.max_points_per_stroke) {
                            throw std::runtime_error("stroke too long");
                        }
                    }
                    page.addObject(std::move(object));
                });
                break;
            case CanvasObject::CanvasObjectEventType::DELETE:
                room.manipulatePage(page_id, [&](Page& page) {
                    page.deleteObject(object_id);
                });
                break;
            default:
                room.manipulatePage(page_id, [&](Page& page) {
                    page.manipulateObject(object_id, [&](CanvasObject& object) {
                        if (event_type == CanvasObject::CanvasObjectEventType::APPEND) {
                            auto* stroke = dynamic_cast<Stroke*>(&object);
                            size_t added = event.at("new_points").size();
                            if (stroke && stroke->pointCount() + added > config.max_points_per_stroke) {
                                throw std::runtime_error("stroke too long");
                            }
                        }
                        object.applyEvent(event);
                    });
                });
                break;
            }
            break;
        }

        default:
            // TEXT, RESET, and unknown types are never accepted from clients.
            throw std::runtime_error("unsupported object type");
        }

        sendToConnectedUsers(room, event.dump(), &sender);
    });

    if (socket_to_kick != nullptr) {
        lws_set_timeout(socket_to_kick, PENDING_TIMEOUT_CLOSE_SEND, LWS_TO_KILL_ASYNC);
    }
}
