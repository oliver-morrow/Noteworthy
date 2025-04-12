#include "ClientWebSocketHandler.hpp"
#include "AppNavigator.hpp"
#include "RoomState.hpp"
#include "Shape.hpp"
#include "Stroke.hpp"
#include "Symbol.hpp"
#include "hosts.hpp"
#include <qgraphicsitem.h>
#include <qwebsockethandshakeoptions.h>

ClientWebSocketHandler::ClientWebSocketHandler(QObject* parent)
    : QObject { parent }
{
    connect(this, &ClientWebSocketHandler::startConnection, this, &ClientWebSocketHandler::openConnection);
}

void ClientWebSocketHandler::openConnection(std::string username, std::string room_id)
{
    closeWebSocket();
    webSocket = std::make_unique<QWebSocket>();
    webSocket->setMaxAllowedIncomingFrameSize(10000000);
    webSocket->setOutgoingFrameSize(10000000);
    connect(webSocket.get(), &QWebSocket::connected, this, &ClientWebSocketHandler::onConnected);
    connect(webSocket.get(), &QWebSocket::disconnected, this, &ClientWebSocketHandler::closed);
    connect(webSocket.get(), QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
        this, &ClientWebSocketHandler::onError); // Handle WebSocket errors
    this->username = username;
    this->room_id = room_id;
    QWebSocketHandshakeOptions options;
    options.setSubprotocols({ "echo-protocol" });
    std::string url_string = NW_WS + "?username=" + username + "&room_id=" + room_id;
    webSocket->open(QUrl::fromUserInput(QString::fromStdString(url_string)), options);
}

void ClientWebSocketHandler::onConnected()
{
    qDebug() << "WebSocket connected";
    connect(webSocket.get(), &QWebSocket::textMessageReceived,
        this, &ClientWebSocketHandler::onTextMessageReceived);
}

void ClientWebSocketHandler::closed()
{
    qDebug() << "WebSocket closed and cleaned up! Error:" << webSocket->errorString();

    closeWebSocket(); // Ensures the socket is fully cleaned up
    emit startConnection(this->username, state.room_id);
}

void ClientWebSocketHandler::onError(QAbstractSocket::SocketError error)
{
    qDebug() << "WebSocket error occurred:" << error;
    closeWebSocket(); // Ensures the socket is fully cleaned up
    emit startConnection(this->username, state.room_id);
}

void ClientWebSocketHandler::onTextMessageReceived(QString message)
{
    try {
        nlohmann::json event = nlohmann::json::parse(message.toStdString());
        handleEvent(event);
    } catch (...) {
        qDebug() << "error handling!!!!";
    }
}

void ClientWebSocketHandler::sendEvent(const nlohmann::json& event)
{
    if (webSocket.get() == nullptr) {
        return;
    }
    webSocket->sendTextMessage(QString::fromStdString(event.dump()));
}

std::unique_ptr<CanvasObject> ClientWebSocketHandler::createCanvasObject(EventObjectType object_type, const nlohmann::json& event, QGraphicsScene& scene, QColor color)
{
    switch (object_type) {
    case STROKE: {
        // Initialize current_path with a new QPainterPath
        auto path = QPainterPath();
        qDebug() << "addedPath: " << path;
        QGraphicsPathItem* item = scene.addPath(path, color);
        // Create a Stroke using the current_path
        auto stroke = std::make_unique<Stroke>(path, item);

        return std::move(stroke);
    }
    case SYMBOL: {
        auto item_type = static_cast<Symbol::SymbolType>(event.at("symbol_type"));
        auto symbol_svg = new QGraphicsSvgItem(Symbol::symbolSvgPaths.at(item_type));
        scene.addItem(symbol_svg);
        return std::make_unique<Symbol>(item_type, symbol_svg);
    }
    case SHAPE: {
        auto item_type = static_cast<Shape::ShapeType>(event.at("shape_type"));
        auto shape_svg = new QGraphicsSvgItem(Shape::shapeSvgPaths.at(item_type));
        scene.addItem(shape_svg);
        return std::make_unique<Shape>(item_type, shape_svg);
    }
    default:
        qDebug() << "Unsupported object type!";
    }
    throw std::runtime_error("unknown object type");
    return nullptr;
}

std::vector<QColor> colors = {
    QColor { 255, 0, 0 },
    QColor { 255, 127, 0 },
    // QColor{255, 255, 0}, remove yellow
    QColor { 0, 255, 0 },
    QColor { 0, 0, 255 },
    QColor { 75, 0, 130 },
    QColor { 148, 0, 211 },
};

QColor stringToColor(const std::string& string)
{
    static const std::vector<QColor> colors = {
        QColor { 255, 0, 0 }, QColor { 255, 127, 0 }, QColor { 0, 255, 0 },
        QColor { 0, 0, 255 }, QColor { 75, 0, 130 }, QColor { 148, 0, 211 }
    };

    int sum = 0;
    for (char c : string)
        sum += c;

    return colors[sum % colors.size()];
}

QPixmap base64ToPixmap(const QString base64String)
{
    // Step 1: Decode the base64 string into a QByteArray
    QByteArray byteArray = QByteArray::fromBase64(base64String.toUtf8());

    // Step 2: Load the decoded byte array into a QPixmap
    QPixmap pixmap;
    if (!pixmap.loadFromData(byteArray)) {
        throw std::runtime_error("error loading byte array decoded from base64");
    }

    return pixmap;
}

void ClientWebSocketHandler::handleEvent(const nlohmann::json& event)
{
    if (event["room_id"] != state.room_id) {
        return;
    } // ignore events from other rooms

    auto object_type = static_cast<EventObjectType>(event["object_type"]);
    switch (object_type) {
    case ROOM: {
        auto event_type = static_cast<RoomState::RoomEventType>(event["event_type"]);
        switch (event_type) {
        case RoomState::RoomEventType::DELETE: {
            throw std::runtime_error("shouldn't be able to delete room");
            break;
        }
        default: {
            state.applyEvent(event);
            break;
        }
        }
        break;
    }
    case PAGE: {
        auto event_type = static_cast<Page::PageEventType>(event["event_type"]);
        switch (event_type) {
        case Page::PageEventType::CREATE: {
            throw std::runtime_error("can only insert page not create");
            break;
        }
        case Page::PageEventType::DELETE: {
            emit pageDeleted(event["page_id"]);
            state.deletePage(event["page_id"]);
            break;
        }
        case Page::PageEventType::INSERT: {
            state.applyInsertPageEvent(event);
            emit pageCreated(event["page_id"]);
            break;
        }
        case Page::PageEventType::INSERT_PDF: {
            qDebug() << "event page id: " << std::to_string(uint64_t(event["page_id"]));
            state.applyInsertPDFPageEvent(event);
            state.manipulatePage(event["page_id"], [&](Page& page) {
                QPixmap pixmap;
                try {
                    pixmap = base64ToPixmap(QString::fromStdString(event["base64_image"]));
                } catch (std::exception) {
                    return;
                }

                // Assuming the scene size is 1000x700
                int sceneWidth = 1000;
                int sceneHeight = 700;

                // Assuming pixmap is already loaded and page.pixmap is a QGraphicsPixmapItem
                QGraphicsPixmapItem* pixmapItem = page.scene->addPixmap(pixmap);
                pixmapItem->setTransformationMode(Qt::SmoothTransformation);
                pixmapItem->setScale(1.25);

                // Calculate the center position
                int x = (sceneWidth - pixmap.width() * 1.25) / 2;
                int y = (sceneHeight - pixmap.height() * 1.25) / 2;

                // Set the pixmap's position to the center
                pixmapItem->setPos(x, y);

                // Now the pixmap is centered in the scene
                page.pixmap = pixmapItem;
            });
            emit pageCreated(event["page_id"]);
            break;
        }
        default: {
            state.manipulatePage(event["page_id"], [&](Page& page) {
                page.applyEvent(event);
            });
            break;
        }
        }

        break;
    }
    case USER: {
        auto event_type = static_cast<User::UserEventType>(event["event_type"]);
        switch (event_type) {
        case User::UserEventType::CREATE: {
            auto user = std::make_unique<User>();
            user->fromJson(event);
            state.addUser(std::move(user));
            break;
        }
        case User::UserEventType::DELETE: {
            throw std::logic_error("shouldn't ever delete user");
            state.removeUser(event["username"]);
            break;
        }
        case User::UserEventType::KICK: {
            if (event["username"] == username) {
                navigator->goToHomepage();
                closeWebSocket();
            }
            break;
        }
        default:
            state.manipulateUser(event["username"], [&](User& user) {
                user.applyEvent(event);
            });
            break;
        }
        emit usersUpdated();
        break;
    }
    case STROKE:
    case SYMBOL:
    case SHAPE:
    case TEXT: {
        auto event_type = static_cast<CanvasObject::CanvasObjectEventType>(event["event_type"]);
        switch (event_type) {
        case CanvasObject::CanvasObjectEventType::CREATE: {
            state.manipulatePage(event["page_id"], [this, event, object_type](Page& page) mutable {
                std::unique_ptr<CanvasObject> object = createCanvasObject(object_type, event, *page.scene, stringToColor(event["owner_id"]));
                object->fromJson(event);
                page.addObject(std::move(object));
            });
            break;
        }
        case CanvasObject::CanvasObjectEventType::DELETE: {
            uint64_t object_id = event["object_id"];
            state.manipulatePage(event["page_id"], [object_id](Page& page) {
                page.manipulateObject(object_id, [](CanvasObject& object) {
                    QGraphicsScene* scene = object.item->scene();
                    if (scene) {
                        scene->removeItem(object.item);
                    }

                    delete object.item;
                    object.item = nullptr;
                });
                page.deleteObject(object_id);
            });
            break;
        }
        default: {
            uint64_t object_id = event["object_id"];
            state.manipulatePage(event["page_id"], [object_id, event](Page& page) {
                page.manipulateObject(object_id, [event](CanvasObject& canvas_object) {
                    canvas_object.applyEvent(event);
                });
            });
            break;
        }
        }
        break;
    }
    case RESET: {
        navigator->goToRoomPageJoin(state.room_id, username, state.password);
        break;
    }
    default:
        qDebug() << "Unknown event type!";
    }
}
