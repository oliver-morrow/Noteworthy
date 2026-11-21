#pragma once

#include "CanvasObject.hpp"
#include "nlohmann/json_fwd.hpp"
#include <QObject>
#include <QTimer>
#include <qgraphicsscene.h>
#include <qwebsocket.h>



class ClientWebSocketHandler : public QObject
{
    Q_OBJECT
public:
    explicit ClientWebSocketHandler(QObject *parent = nullptr);
    void handleEvent(const nlohmann::json& event);
    void sendEvent(const nlohmann::json& event);
    void closeWebSocket();

    std::unique_ptr<CanvasObject> createCanvasObject(EventObjectType object_type, const nlohmann::json &event, QGraphicsScene& scene, QColor color = QColor{255, 50, 50});

signals:
    void pageCreated(uint64_t page_id);
    void pageDeleted(uint64_t page_id);
    void startConnection(std::string username, std::string room_id, std::string session_token);
    void usersUpdated();

private Q_SLOTS:
    void openConnection(std::string username, std::string room_id, std::string session_token);
    void onConnected();
    void closed();
    void onTextMessageReceived(QString message);
    void onError(QAbstractSocket::SocketError error);


private:
    std::unique_ptr<QWebSocket> webSocket;
    std::string username;
    std::string room_id;
    std::string session_token;
    QTimer reconnect_timer;
};
