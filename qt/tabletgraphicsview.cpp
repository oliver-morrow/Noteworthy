#include "tabletgraphicsview.hpp"
#include "RoomState.hpp"
#include <QWebSocketHandshakeOptions>
#include "RandomIdGenerator.hpp"

#include "Stroke.hpp"

using json = nlohmann::json; // Define a shorthand for the json type

void TabletGraphicsView::handleTouch(QPointF position, int id)
{
    // qDebug() << "Touch pressed at:" << position << "with ID:" << id;

    // ignore touches if not on a page
    if (current_page_id == 0)
    {
        return;
    }
    // only care about 1 touch
    QPointF scene_pos = this->mapToScene(position.toPoint());
    touch_state.updateTouch(position, id);

    // stupid hard coding
    if (touch_state.isPinching() && (centerPoint.x() <= 0 || centerPoint.y() <= 0 || centerPoint.x() > 1000 || centerPoint.x() > 700)) {
        centerPoint = touch_state.getPinchCenterPoint();
        touch_state.setPrevCenterPoint(centerPoint);
    }

    if (touch_state.isPinching()) {
        touch_state.setPrevCenterPoint(touch_state.getPinchCenterPoint());
    }

    switch (touch_state.current_touch_action)
    {
    case TouchState::APPEND_STROKE:
    {
        auto current_path = QPainterPath(scene_pos);
        // Create a Stroke using the current_path
        current_stroke = std::make_unique<Stroke>(current_path);
        current_stroke->room_id = state.room_id;
        current_stroke->page_id = current_page_id;
        current_stroke->owner_id = user_id;
        current_stroke->object_id = IDGenerator::newID();

        nlohmann::json event;
        current_stroke->createCreateEvent(event);
        ws_handler.sendEvent(event);
        ws_handler.handleEvent(event);
        break;
    }
    case TouchState::ERASE_STROKE: {
        erase_path = std::make_unique<QPainterPath>(scene_pos);
        break;
    }
    case TouchState::DRAG_SELECTION: {
        selection.page_id = current_page_id;
        qDebug() << "We selectin";
        selection.initSelDragBox(state.getScene(current_page_id), scene_pos);
        selection.selecting = true;
        qDebug() << position << "\n";
        break;
    }
    case TouchState::DRAG_HANDLE:
        selection.page_id = current_page_id;
        auto clicked = scene()->items(scene_pos);
        for (auto item : clicked) {
            qDebug() << "checking for clicked handle";

            // i would like to apologize in advance
            if(item == selection.sel_N_canvas) {
                qDebug() << "north drag box";
            }
            else if (item == selection.sel_E_canvas) {
                qDebug() << "E";
            }
            else if (item == selection.sel_S_canvas){
                qDebug() << "S";
            }
            else if (item == selection.sel_W_canvas){
                qDebug() << "W";
            }
            else if (item == selection.sel_NW_canvas){
                qDebug() << "NW";
            }
            else if (item == selection.sel_NE_canvas){
                qDebug() << "NE";
            }
            else if (item == selection.sel_SW_canvas){
                qDebug() << "SW";
            }
            else if (item == selection.sel_SE_canvas){
                qDebug() << "SE";
            }
            else if (item == selection.rect_item){
                qDebug() << "box";
                if(selection.current_transform == NONE){
                    qDebug() << "we are translating now";
                    selection.previous_move = scene_pos;
                    selection.current_transform = TRANSLATE;
                }
            }
        }
        if (selection.current_transform == NONE){
            qDebug() << "exit selection";
            selection.hideSelBox();
            selection.sel_list.clear();
        }
        break;
    };
}

void TabletGraphicsView::updateViewFromTouchState() {
    if (!touch_state.isPinching()) {
        return; // No pinch gesture detected
    }

    // Get the zoom ratio and relative center from the touch state
    double zoomRatio = touch_state.getZoomRatio();
    QPointF relative_center = touch_state.getRelativeCenter();
    QPointF pinch_center = touch_state.getPinchCenterPoint();
    qDebug() << pinch_center;
    // to scale about the center point
    centerOn(pinch_center.x(), pinch_center.y());
    scale(zoomRatio, zoomRatio);

    double absolute_scale = transform().m11();

    centerPoint.setX(centerPoint.x()-relative_center.x()/absolute_scale);
    centerPoint.setY(centerPoint.y()-relative_center.y()/absolute_scale);

    // to actually move it
    centerOn(centerPoint.x(), centerPoint.y());
}

void TabletGraphicsView::handleMove(QPointF position, int id)
{
    if (current_page_id == 0)
    {
        return;
    }

    QPointF scene_pos = this->mapToScene(position.toPoint());
    touch_state.updateTouch(position, id);

    if (touch_state.isPinching()) {
        updateViewFromTouchState();
        return;
    }

    switch (touch_state.current_touch_action)
    {
    case TouchState::APPEND_STROKE:
    {
        if (!current_stroke) // mouse is just moving around
        {
            return;
        }

        bool stroke_exists = false;
        state.manipulatePage(current_page_id, [&](Page& page){
            stroke_exists = page.hasObject(current_stroke->object_id);
        });

        if (!stroke_exists) {
            current_stroke = nullptr;
            current_stroke_id = 0;
            return;
        }

        current_stroke->path.lineTo(scene_pos); // Extend the path to the new point
        nlohmann::json event_json;
        current_stroke->createAppendEvent(event_json, {{scene_pos.x(), scene_pos.y()}}); // Update the QGraphicsPathItem
        // current_stroke->applyAppendEvent(event_json);

        ws_handler.sendEvent(event_json);
        ws_handler.handleEvent(event_json);
        break;
    }
    case TouchState::ERASE_STROKE:
    {
        if (!erase_path) // mouse is just moving around
        {
            return;
        }
        qDebug() << "ooo stroke delete!";

        erase_path->lineTo(scene_pos);

        auto item_list = scene()->items(*erase_path);
        for (auto item : item_list) {
            qDebug() << "moved!";
            // Remove the item from the scene
            bool was_real_object = false;
            nlohmann::json event_json;
            state.manipulatePage(current_page_id, [&was_real_object, item, &event_json](Page& page){
                uint64_t object_id = page.getObjectIdFromGraphicsItem(item);
                if (object_id == -1) {
                    return;
                }
                page.manipulateObject(object_id, [&event_json](CanvasObject& object){
                    object.createDeleteEvent(event_json);
                });
                was_real_object = true;
            });

            if (!was_real_object) {
                continue;
            }

            ws_handler.sendEvent(event_json);
            ws_handler.handleEvent(event_json);
        }
        break;
    }
    case TouchState::DRAG_SELECTION: {
        if (selection.selecting)
        {
            qDebug() << "We draggin now";
            selection.updateSelDragBox(scene_pos);
        }
        break;
    }
    case TouchState::DRAG_HANDLE:
        switch(selection.current_transform)
        {
        case NONE:{
            break;
        }
        case TRANSLATE:{
            selection.hideSelBox();
            qDebug() << "currently translating";
            selection.moveSelection(scene_pos.x()-selection.previous_move.x(), scene_pos.y()-selection.previous_move.y(), this->ws_handler);
            qDebug() << scene_pos.x()-selection.previous_move.x();
            qDebug() << scene_pos.y()-selection.previous_move.y();
            selection.previous_move = scene_pos;
            break;
        }
        case SCALE:{
            break;
        }
        }
        break;
    }
}

void TabletGraphicsView::handleRelease(QPointF position, int id)
{
    if (current_page_id == 0)
    {
        return;
    }
    // qDebug() << "Touch released at:" << position << "with ID:" << id;
    // only care about 1 touch

    QPointF scene_pos = this->mapToScene(position.toPoint());
    touch_state.removeTouch(id);

    erase_path = nullptr;

    switch (touch_state.current_touch_action)
    {
    case TouchState::APPEND_STROKE:
    {
        if (!(current_stroke))
        {
            qDebug() << "what?? release when there the stroke is null";
            return;
        }
        current_stroke->path.lineTo(scene_pos); // Extend the path to the new point
        nlohmann::json event_json;
        current_stroke->createAppendEvent(event_json, {{scene_pos.x(), scene_pos.y()}}); // Update the QGraphicsPathItem
        current_stroke->applyAppendEvent(event_json);

        ws_handler.sendEvent(event_json);
        ws_handler.handleEvent(event_json);

        current_stroke = nullptr;
        current_stroke_id = 0;
        break;
    }
    case TouchState::ERASE_STROKE:{
        break;
    }
    case TouchState::DRAG_SELECTION: {
        selection.selecting = false;
        qDebug() << "we no longer dragging";
        auto item_list = scene()->items(selection.drag_box->rect());
        qDebug() << "we got da items (hopefully)";
        std::list<uint64_t> sel_add;

        for (auto item : item_list) {
            state.manipulatePage(current_page_id, [item, &sel_add](Page& page){
                //if(item != )
                uint64_t object_id = page.getObjectIdFromGraphicsItem(item);
                if (object_id != -1){
                    page.manipulateObject(object_id, [&](CanvasObject& object){
                        if (object.getObjectType() == STROKE) {
                            sel_add.push_back(object_id);
                            qDebug() << "now adding " << object_id;
                        }
                    });
                }
            });
        }
        selection.addSelection(sel_add);
        qDebug() << "successfully added";
        selection.drag_box->hide();
        if(selection.sel_list.size() > 0)
        {
            qDebug() << "there is at least 1 element selected";
            touch_state.current_touch_action = TouchState::DRAG_HANDLE;
            selection.drawSelBox(state.getScene(current_page_id));
        }
        break;
    }
    case TouchState::DRAG_HANDLE:
        if (selection.sel_list.empty()){
            touch_state.current_touch_action = TouchState::DRAG_SELECTION;
        }
        else{
            selection.current_transform = NONE;
            selection.updateSelBox();
            selection.drawSelBox(state.getScene(current_page_id));
        }
        break;
    }
}

void TabletGraphicsView::resizeEvent(QResizeEvent *event)
{
    // Update the scene rectangle to match the new size of the view
    qDebug() << "Resize! width: "<<this->width()<<", height: "<<this->height();
    QGraphicsView::resizeEvent(event); // Call the base class implementation (optional if you don't want any default behavior)
    // this->setSceneRect(0, 0, this->width() - 5, this->height() - 5);
}
