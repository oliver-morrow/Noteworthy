#include "AppNavigator.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_JS(void, loadRoomSession, (const char* key, char* output), {
    try {
        stringToUTF8(sessionStorage.getItem(UTF8ToString(key)) || "", output, 65);
    } catch (error) {
        // The browser can disable storage. Keep using the in-memory session.
    }
});

EM_JS(void, storeRoomSession, (const char* key, const char* token), {
    try {
        const name = UTF8ToString(key);
        const value = UTF8ToString(token);
        if (value) {
            sessionStorage.setItem(name, value);
        } else {
            sessionStorage.removeItem(name);
        }
    } catch (error) {
        // The in-memory session still supports reconnecting in this page.
    }
});
#endif

namespace {
QString sessionKey(const std::string& room_id, const std::string& user_id)
{
    return QString::fromStdString("noteworthy.room." + room_id + "." + user_id);
}
}

std::string AppNavigator::savedSession(const std::string& room_id, const std::string& user_id)
{
    const auto key = sessionKey(room_id, user_id);
    const auto saved = room_sessions.value(key);
    if (!saved.isEmpty()) {
        return saved.toStdString();
    }
#ifdef __EMSCRIPTEN__
    char token[65] = {};
    loadRoomSession(key.toUtf8().constData(), token);
    return token;
#else
    return "";
#endif
}

void AppNavigator::saveSession(const std::string& room_id, const std::string& user_id, const std::string& token)
{
    const auto key = sessionKey(room_id, user_id);
    if (token.empty()) {
        room_sessions.remove(key);
    } else {
        room_sessions.insert(key, QString::fromStdString(token));
    }
#ifdef __EMSCRIPTEN__
    storeRoomSession(key.toUtf8().constData(), token.c_str());
#endif
}

AppNavigator* navigator;
