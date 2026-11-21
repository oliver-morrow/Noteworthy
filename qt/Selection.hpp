#pragma once

#include "ClientWebSocketHandler.hpp"
#include "RoomState.hpp"
#include <QGraphicsRectItem>
#include <QPointer>
#include <list>

class Selection {
public:
    ~Selection() { clear(); }

    void clear()
    {
        if (scene) {
            delete frame;
            delete marquee;
        }
        scene.clear();
        frame = nullptr;
        marquee = nullptr;
        objects.clear();
        moving = false;
    }

    void selectObject(QGraphicsScene* canvas, uint64_t page, uint64_t object)
    {
        clear();
        scene = canvas;
        page_id = page;
        objects.push_back(object);
        updateFrame();
    }

    void press(QGraphicsScene* canvas, uint64_t page, const QPointF& position)
    {
        if (scene == canvas && frame && frame->rect().contains(position)) {
            moving = true;
            previous_position = position;
            return;
        }

        clear();
        scene = canvas;
        page_id = page;
        previous_position = position;

        state.manipulatePage(page_id, [&](Page& page) {
            for (auto* item : canvas->items(position)) {
                const auto object = page.getObjectIdFromGraphicsItem(item);
                if (object != uint64_t(-1)) {
                    objects.push_back(object);
                    break;
                }
            }
        });

        if (objects.empty()) {
            marquee = addOutline(Qt::DashLine);
        } else {
            moving = true;
            updateFrame();
        }
    }

    void drag(const QPointF& position, ClientWebSocketHandler& socket)
    {
        if (!scene) {
            return;
        }
        if (marquee) {
            marquee->setRect(QRectF(previous_position, position).normalized());
        } else if (moving && position != previous_position) {
            const auto delta = position - previous_position;
            previous_position = position;
            for (auto id : objects) {
                nlohmann::json event;
                state.manipulatePage(page_id, [&](Page& page) {
                    page.manipulateObject(id, [&](CanvasObject& object) {
                        object.createMoveEvent(event, delta.x(), delta.y());
                    });
                });
                if (!event.is_null()) {
                    socket.sendEvent(event);
                    socket.handleEvent(event);
                }
            }
            updateFrame();
        }
    }

    void release(const QPointF& position, ClientWebSocketHandler& socket)
    {
        drag(position, socket);
        moving = false;
        if (!scene || !marquee) {
            return;
        }
        const auto region = marquee->rect();
        delete marquee;
        marquee = nullptr;
        if (region.isEmpty()) {
            return;
        }
        state.manipulatePage(page_id, [&](Page& page) {
            for (auto* item : scene->items(region)) {
                const auto object = page.getObjectIdFromGraphicsItem(item);
                if (object != uint64_t(-1)) {
                    objects.push_back(object);
                }
            }
        });
        updateFrame();
    }

    void refresh(uint64_t page)
    {
        if (scene && page == page_id && frame) {
            updateFrame();
        }
    }

private:
    QGraphicsRectItem* addOutline(Qt::PenStyle style)
    {
        QPen pen(QColor("#358944"), 1, style);
        pen.setCosmetic(true);
        auto* outline = scene->addRect({}, pen);
        outline->setZValue(1);
        return outline;
    }

    void updateFrame()
    {
        QRectF bounds;
        bool exists_in_bounds = false;
        state.manipulatePage(page_id, [&](Page& page) {
            objects.remove_if([&](uint64_t id) {
                bool exists = false;
                page.manipulateObject(id, [&](CanvasObject& object) {
                    const auto item_bounds = object.item->sceneBoundingRect();
                    bounds = exists_in_bounds ? bounds.united(item_bounds) : item_bounds;
                    exists_in_bounds = true;
                    exists = true;
                });
                return !exists;
            });
        });
        if (objects.empty()) {
            delete frame;
            frame = nullptr;
            moving = false;
            return;
        }
        if (!frame) {
            frame = addOutline(Qt::SolidLine);
        }
        frame->setRect(bounds.adjusted(-4, -4, 4, 4));
    }

    QPointer<QGraphicsScene> scene;
    QGraphicsRectItem* frame = nullptr;
    QGraphicsRectItem* marquee = nullptr;
    uint64_t page_id = 0;
    std::list<uint64_t> objects;
    QPointF previous_position;
    bool moving = false;
};
