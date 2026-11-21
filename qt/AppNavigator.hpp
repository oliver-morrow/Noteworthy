#pragma once

#include "RoomState.hpp"
#include "drawingroom.h"
#include "hosts.hpp"
#include "loadingroompage.h"
#include "newuser.h"
#include "widget.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QUrlQuery>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

class AppNavigator {
public:
    // Homepage homepage;
    // WelcomePage welcome_page;
    // RoomPage room_page;
    NewUser welcome_page;
    Widget homepage;
    drawingRoom room_page;
    LoadingRoomPage loading_room_page;

    void goToWelcomePage()
    {
        ++room_request;
        welcome_page.show();
        homepage.hide();
        room_page.hide();
        loading_room_page.hide();
    }

    void goToHomepage(const QString& user_id)
    {
        homepage.setUsername(user_id);
        goToHomepage();
    }

    void goToHomepage()
    {
        ++room_request;
        homepage.show();
        welcome_page.hide();
        room_page.hide();
        loading_room_page.hide();
    }

    void goToRoomPageCreate(const std::string& user_id)
    {
        const auto request = ++room_request;
        room_page.closeWebSocket();
        createRoom(user_id, [this, request, user_id](std::string body, HttpStatusCode code) {
            if (request != room_request) {
                return;
            }
            if (code != Created) {
                goToHomepage();
                homepage.setErrorText(errorText(code, "Failed to create room"));
                return;
            }
            const auto session = QJsonDocument::fromJson(QByteArray::fromStdString(body)).object();
            const auto room_id = session.value("room_id").toString().toStdString();
            const auto token = session.value("session_token").toString().toStdString();
            openRoom(request, room_id, user_id, token);
        });
    }

    enum HttpStatusCode {
        InvalidCode = -1,
        // Success codes
        OK = 200,
        Created = 201,

        // Client error responses
        BadRequest = 400,
        Unauthorized = 401,
        Forbidden = 403,
        NotFound = 404,
        Conflict = 409,
        Locked = 423,
        TooManyRequests = 429
    };

    void goToLoadingRoomPage(const std::string& room_id, const std::string& user_id)
    {

        loading_room_page.room_id = room_id;
        loading_room_page.setNameLabel(QString::fromStdString(user_id));
        loading_room_page.user_id = user_id;
        loading_room_page.show();
        homepage.hide();
        welcome_page.hide();
        room_page.hide();
    }

    void goToRoomPageJoin(const std::string& room_id, const std::string& user_id, const std::string& password = "")
    {
        const auto request = ++room_request;
        room_page.closeWebSocket();
        const auto token = savedSession(room_id, user_id);
        if (!token.empty()) {
            openRoom(request, room_id, user_id, token);
            return;
        }

        createUserConnection(user_id, room_id, password, [this, request, room_id, user_id](std::string body, HttpStatusCode code) {
            if (request != room_request) {
                return;
            }
            switch (code) {
            case Created: {
                const auto session = QJsonDocument::fromJson(QByteArray::fromStdString(body)).object();
                openRoom(request, room_id, user_id, session.value("session_token").toString().toStdString());
                return;
            }
            case Locked:
                goToLoadingRoomPage(room_id, user_id);
                return;
            case Forbidden:
                if (body == "Incorrect Password") {
                    loading_room_page.setErrorText("Incorrect password");
                    return;
                }
                break;
            default:
                break;
            }
            goToHomepage();
            if (code == Conflict) {
                homepage.setErrorText("That name is already reserved in this room");
            } else if (code == NotFound) {
                homepage.setErrorText("Invalid room code");
            } else if (code == Forbidden) {
                homepage.setErrorText(QString::fromStdString(body));
            } else {
                homepage.setErrorText(errorText(code, "Failed to join room"));
            }
        });
    }

private:
    quint64 room_request = 0;
    QHash<QString, QString> room_sessions;

    std::string savedSession(const std::string& room_id, const std::string& user_id);
    void saveSession(const std::string& room_id, const std::string& user_id, const std::string& token);

    static QString errorText(HttpStatusCode code, const QString& fallback)
    {
        if (code == TooManyRequests) {
            return "Too many requests. Try again shortly";
        }
        if (code == InvalidCode || code >= 500) {
            return "Noteworthy server unavailable. Try again later";
        }
        return fallback;
    }

    void openRoom(quint64 request, const std::string& room_id, const std::string& user_id, const std::string& token)
    {
        if (room_id.empty() || token.size() != 64) {
            goToHomepage();
            homepage.setErrorText("The server did not issue a room session");
            return;
        }
        saveSession(room_id, user_id, token);
        getRoomState(room_id, token, [this, request, room_id, user_id, token](std::string body, HttpStatusCode code) {
            if (request != room_request) {
                return;
            }
            if (code != OK) {
                if (code == Unauthorized || code == NotFound) {
                    saveSession(room_id, user_id, "");
                }
                goToHomepage();
                homepage.setErrorText(errorText(code, code == Unauthorized ? "This room session has ended" : "Could not load the room"));
                return;
            }
            state.room_id = room_id;
            room_page.user_id = user_id;
            room_page.setUser(QString::fromStdString(user_id).at(0));
            room_page.initialize(body, token);
            room_page.show();
            homepage.hide();
            welcome_page.hide();
            loading_room_page.hide();
        });
    }

    QNetworkAccessManager manager;

    enum class HttpMethod {
        GET,
        POST
    };

    using ApiResponseCallback = std::function<void(std::string, HttpStatusCode)>;

    void makeRequest(
        HttpMethod method,
        const QUrl& url,
        const std::string& authorization,
        const ApiResponseCallback& onReply)
    {
        QNetworkRequest request(url);

        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
        request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
        if (!authorization.empty()) {
            request.setRawHeader("Authorization", "Bearer " + QByteArray::fromStdString(authorization));
        }

        // Determine the HTTP method
        QNetworkReply* reply = nullptr;
        if (method == HttpMethod::POST) {
            reply = manager.post(request, QByteArray()); // POST
        } else if (method == HttpMethod::GET) {
            reply = manager.get(request); // GET
        }

        QObject::connect(reply, &QNetworkReply::finished, reply, [reply, onReply]() {
            const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
            onReply(reply->readAll().toStdString(),
                status.isValid() ? static_cast<HttpStatusCode>(status.toInt()) : InvalidCode);
            reply->deleteLater();
        });
    }

    static QUrl withUsername(const std::string& endpoint, const std::string& username)
    {
        QUrl url(QString::fromStdString(endpoint));
        QUrlQuery query;
        query.addQueryItem("username", QString::fromStdString(username));
        url.setQuery(query);
        return url;
    }

    // Create a room
    void createRoom(
        const std::string& owner_id,
        const ApiResponseCallback& onReply)
    {
        const auto url = withUsername(NW_HTTP + "/v1/rooms", owner_id);
        makeRequest(HttpMethod::POST, url, "", onReply);
    }

    // Create a user connection
    void createUserConnection(
        const std::string& username,
        const std::string& room_id,
        const std::string& password,
        const ApiResponseCallback& onReply)
    {
        const auto url = withUsername(NW_HTTP + "/v1/rooms/" + room_id + "/users", username);
        makeRequest(HttpMethod::POST, url, password, onReply);
    }

    // Get room state
    void getRoomState(
        const std::string& room_id,
        const std::string& token,
        const ApiResponseCallback& onReply)
    {
        QUrl url(QString::fromStdString(NW_HTTP + "/v1/rooms/" + room_id));
        makeRequest(HttpMethod::GET, url, token, onReply);
    }
};

extern AppNavigator* navigator;
