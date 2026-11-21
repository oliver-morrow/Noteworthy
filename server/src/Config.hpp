#pragma once

#include <cstddef>
#include <cstdlib>
#include <string>

// Runtime limits, read once from the environment. Defaults are sized for a
// small public demo; override them in compose.yaml.
struct Config {
    bool enable_pdf; // NW_ENABLE_PDF=1 turns PDF import on (off by default)
    bool debug_log; // NW_DEBUG_LOG=1 logs every WebSocket message
    size_t max_rooms;
    size_t room_idle_minutes; // rooms with nobody connected are evicted after this
    size_t max_users_per_room;
    size_t max_pages_per_room;
    size_t max_objects_per_page;
    size_t max_points_per_stroke;
    double ws_messages_per_second;
    double ws_message_burst;
    size_t max_pdf_pages;
    size_t max_pdf_page_pixels; // width * height of one rendered page

    static const Config& get()
    {
        static const Config config = load();
        return config;
    }

private:
    static size_t envSize(const char* name, size_t fallback)
    {
        const char* value = std::getenv(name);
        if (!value || !*value) {
            return fallback;
        }
        try {
            return static_cast<size_t>(std::stoull(value));
        } catch (...) {
            return fallback;
        }
    }

    static bool envFlag(const char* name)
    {
        const char* value = std::getenv(name);
        return value && std::string(value) == "1";
    }

    static Config load()
    {
        Config c;
        c.enable_pdf = envFlag("NW_ENABLE_PDF");
        c.debug_log = envFlag("NW_DEBUG_LOG");
        c.max_rooms = envSize("NW_MAX_ROOMS", 200);
        c.room_idle_minutes = envSize("NW_ROOM_IDLE_MINUTES", 120);
        c.max_users_per_room = envSize("NW_MAX_USERS_PER_ROOM", 16);
        c.max_pages_per_room = envSize("NW_MAX_PAGES_PER_ROOM", 50);
        c.max_objects_per_page = envSize("NW_MAX_OBJECTS_PER_PAGE", 5000);
        c.max_points_per_stroke = envSize("NW_MAX_POINTS_PER_STROKE", 10000);
        c.ws_messages_per_second = static_cast<double>(envSize("NW_WS_MESSAGES_PER_SECOND", 120));
        c.ws_message_burst = static_cast<double>(envSize("NW_WS_MESSAGE_BURST", 240));
        c.max_pdf_pages = envSize("NW_MAX_PDF_PAGES", 20);
        c.max_pdf_page_pixels = envSize("NW_MAX_PDF_PAGE_PIXELS", 2000 * 2000);
        return c;
    }
};

// Usernames come from query strings; keep them short and printable.
inline bool isValidUsername(const std::string& username)
{
    if (username.empty() || username.size() > 32) {
        return false;
    }
    for (unsigned char c : username) {
        if (c < 0x20 || c == 0x7f) {
            return false;
        }
    }
    return true;
}
