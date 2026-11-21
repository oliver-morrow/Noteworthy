#pragma once

#include "RandomIdGenerator.hpp"
#include "RoomState.hpp"
#include "Shape.hpp"
#include "Stroke.hpp"
#include "Symbol.hpp"
#include <chrono>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory> // Include for smart pointers
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <variant>
#include <vector>

#include "nlohmann/ordered_map.hpp"

class ServerFullError : public std::runtime_error {
public:
    ServerFullError()
        : std::runtime_error("room limit reached") { }
};

class ServerState {
public:
    std::string createRoom(const std::string& owner, const std::string& session_token, size_t max_rooms)
    {
        std::lock_guard<std::mutex> lock(room_map_mutex);
        if (room_map.size() >= max_rooms) {
            throw ServerFullError();
        }

        std::string new_room_id(8, '0');
        do {
            auto random_bits = IDGenerator::newID();
            for (char& letter : new_room_id) {
                letter = valid_room_id_chars[random_bits & 31];
                random_bits >>= 5;
            }
        } while (room_map.count(new_room_id) > 0);

        room_map[new_room_id] = std::make_unique<RoomState>(new_room_id, owner, session_token);
        return new_room_id;
    }

    void deleteRoom(const std::string& id)
    {
        std::lock_guard<std::mutex> lock(room_map_mutex);
        room_map.erase(id); // Automatic memory management by unique_ptr
    }

    void manipulateRoom(const std::string& id,
        const std::function<void(RoomState&)>& manipulator)
    {
        std::lock_guard<std::mutex> lock(room_map_mutex);
        auto it = room_map.find(id);
        if (it == room_map.end()) {
            throw std::range_error("room not found");
        }
        it->second->last_activity = std::chrono::steady_clock::now();
        manipulator(*it->second); // Pass to manipulator by reference
    }

    bool hasRoom(const std::string& id)
    {
        std::lock_guard<std::mutex> lock(room_map_mutex);
        return room_map.count(id) > 0;
    }

    // Must run on the WebSocket service thread: connected users' sessions hold
    // raw User pointers into their room, so only rooms with nobody connected
    // are removed.
    size_t evictIdleRooms(std::chrono::minutes max_idle)
    {
        std::lock_guard<std::mutex> lock(room_map_mutex);
        auto now = std::chrono::steady_clock::now();
        size_t evicted = 0;
        for (auto it = room_map.begin(); it != room_map.end();) {
            RoomState& room = *it->second;
            if (!room.hasConnectedUsers() && now - room.last_activity > max_idle) {
                it = room_map.erase(it);
                evicted++;
            } else {
                ++it;
            }
        }
        return evicted;
    }

private:
    static constexpr char valid_room_id_chars[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

    std::mutex room_map_mutex;
    std::unordered_map<std::string, std::unique_ptr<RoomState>>
        room_map; // Use unique_ptr for automatic memory management
};

extern ServerState state;
