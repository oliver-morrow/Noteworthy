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
        homepage.show();
        welcome_page.hide();
        room_page.hide();
        loading_room_page.hide();
    }

    void goToRoomPageCreate(const std::string& user_id)
    {
        // initialize room page
        room_page.user_id = user_id;
        createRoom(user_id, [user_id, this](std::string responseString, HttpStatusCode code) {
            // epic
            switch (code) {
            case Created:
                state.room_id = responseString;
                getRoomState(responseString, "", [this](std::string responseString, HttpStatusCode code) {
                    switch (code) {
                    case OK:
                        room_page.show();
                        room_page.initialize(responseString);
                        homepage.hide();
                        welcome_page.hide();
                        loading_room_page.hide();
                        break;
                    default:
                        break;
                    }
                });
                break;
            default:
                homepage.setErrorText("Failed to create room");
                break;
            }
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
        Locked = 423
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

    void goToRoomPageJoin(const std::string& room_id, const std::string& user_id, const std::string password = "")
    {
        room_page.closeWebSocket();
        state.room_id = room_id;
        room_page.user_id = user_id;
        room_page.setCodeLabel(QString::fromStdString(room_id));
        room_page.setUser(QString::fromStdString(user_id).at(0));

        createUserConnection(user_id, room_id, password, [room_id, user_id, password, this](std::string responseString, HttpStatusCode code) {
            switch (code) {
            case Created:
            case OK:
                getRoomState(room_id, password, [room_id, user_id, password, this](std::string responseString, HttpStatusCode code) {
                    switch (code) {
                    case OK:
                        room_page.show();
                        room_page.initialize(responseString);
                        homepage.hide();
                        welcome_page.hide();
                        loading_room_page.hide();
                        break;
                    default:
                        break;
                    }
                });

                break;
            case Locked: {
                goToLoadingRoomPage(room_id, user_id);
                break;
            }
            case Unauthorized:
                throw "auth header is incorrect";
                break;
            case Forbidden:
                if (responseString == "Incorrect Password") {
                    loading_room_page.setErrorText("Incorrect Password");
                } else {
                    homepage.setErrorText("You are no longer allowed in this room");
                }
                break;
            case NotFound:
                homepage.setErrorText("Invalid room code");
                break;
            case Conflict:
                goToHomepage();
                homepage.setErrorText("Username already connected to room");
                break;
            case BadRequest:
            case InvalidCode:
                throw std::runtime_error("invalid request situation");
                break;
            }
        });
    }

    QNetworkAccessManager manager;

    enum class HttpMethod {
        GET,
        POST
    };

public:
    using ApiResponseCallback = std::function<void(std::string, HttpStatusCode)>;

    void makeRequest(
        HttpMethod method,
        const QUrl& url,
        const std::string& password = "",
        const ApiResponseCallback& onReply = nullptr)
    {
        QNetworkRequest request(url);

        // Set headers
        if (!password.empty()) {
            request.setRawHeader("Authorization", QByteArray("Bearer " + QString::fromStdString(password).toUtf8()));
        }

        // Determine the HTTP method
        QNetworkReply* reply = nullptr;
        if (method == HttpMethod::POST) {
            reply = manager.post(request, QByteArray()); // POST
        } else if (method == HttpMethod::GET) {
            reply = manager.get(request); // GET
        }

        QObject* tempContext = new QObject(reply); // Temporary context

        // Handle the reply
        QObject::connect(reply, &QNetworkReply::finished, tempContext, [tempContext, this, reply, onReply]() {
            QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

            if (!statusCode.isValid() || statusCode.toInt() >= 500) {
                homepage.setErrorText("Noteworthy server not online, please try again later");
                reply->deleteLater(); // Clean up
                return;
            }

            QByteArray responseData = reply->readAll();

            HttpStatusCode httpStatusCode = static_cast<HttpStatusCode>(statusCode.isValid() ? statusCode.toInt() : -1);
            std::string responseString = responseData.toStdString();

            if (onReply) {
                onReply(responseString, httpStatusCode);
            }

            reply->deleteLater(); // Clean up reply
            tempContext->deleteLater(); // Clean up context
        }); // !!ADD CONTEXT OBJECT HERE!!!!
    }

    // Create a room
    void createRoom(
        const std::string& owner_id,
        const ApiResponseCallback& onReply)
    {
        QUrl url(QString::fromStdString(NW_HTTP + "/v1/rooms?username=" + owner_id));
        makeRequest(HttpMethod::POST, url, "", onReply);
    }

    // Create a user connection
    void createUserConnection(
        const std::string& username,
        const std::string& room_id,
        const std::string& password,
        const ApiResponseCallback& onReply)
    {
        QUrl url(QString::fromStdString(NW_HTTP + "/v1/rooms/" + room_id + "/users?username=" + username));
        makeRequest(HttpMethod::POST, url, password, onReply);
    }

    // Get room state
    void getRoomState(
        const std::string& room_id,
        const std::string& password,
        const ApiResponseCallback& onReply)
    {
        QUrl url(QString::fromStdString(NW_HTTP + "/v1/rooms/" + room_id));
        makeRequest(HttpMethod::GET, url, password, onReply);
    }

    // void fetchRoomWithPassword
};

extern AppNavigator* navigator;
