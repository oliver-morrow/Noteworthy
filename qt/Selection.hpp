#pragma once

#include "CanvasObject.hpp"
#include "RoomState.hpp"
#include "tabletgraphicsview.hpp"
#include <list>
#include "ClientWebSocketHandler.hpp"
#include <qgraphicsitem.h>
#include <qgraphicsscene.h>

enum SelDragPoint
{
    NW,
    N,
    NE,
    W,
    E,
    SW,
    SE,
    S
};

enum TransformType
{
    NONE,
    TRANSLATE,
    SCALE
};

class Selection
{
public:
    std::string room_id;

           // Points that the user click/drags when making selection
    QPointF init_sel_point;
    bool selecting = false;
    TransformType current_transform = NONE;
    QPointF previous_move;

           // i am going insane
    double max_x = 0;
    double max_y = 0;
    double min_x = INFINITY;
    double min_y = INFINITY;
    double midpoint_x = 0;
    double midpoint_y = 0;

    QRectF bounding_box = QRectF(0,0,0,0);
    QGraphicsRectItem* rect_item; // this one is for the bounding box
    QGraphicsRectItem* drag_box; // this one is for when the user is creating the selection box

           // Selection drag points
    QRectF sel_NW = QRectF(0,0,5,5);
    QRectF sel_NE = QRectF(0,0,5,5);
    QRectF sel_SW = QRectF(0,0,5,5);
    QRectF sel_SE = QRectF(0,0,5,5);
    QRectF sel_N = QRectF(0,0,5,5);
    QRectF sel_W = QRectF(0,0,5,5);
    QRectF sel_E = QRectF(0,0,5,5);
    QRectF sel_S = QRectF(0,0,5,5);

           // there is probably a better way to do this
    QGraphicsRectItem* sel_NW_canvas;
    QGraphicsRectItem* sel_NE_canvas;
    QGraphicsRectItem* sel_SW_canvas;
    QGraphicsRectItem* sel_SE_canvas;
    QGraphicsRectItem* sel_N_canvas;
    QGraphicsRectItem* sel_E_canvas;
    QGraphicsRectItem* sel_W_canvas;
    QGraphicsRectItem* sel_S_canvas;

    uint64_t page_id;

           // selection box styles
    QPen sel_box_pen = QPen(Qt::PenStyle::SolidLine);
    QPen drag_box_pen = QPen(Qt::PenStyle::SolidLine);

           // this should be a map since we assume unique object ids
    std::list<uint64_t> sel_list;

    void initSelDragBox(std::shared_ptr<QGraphicsScene> scene, QPointF init_point)
    {
        qDebug() << "Real!!!!!!!!!!";
        drag_box = scene->addRect(QRectF(init_point, init_point));
        drag_box->setPen(drag_box_pen);
        qDebug() << "Behold, the rectangle is summoned";
        init_sel_point = init_point;
        drag_box->show();
        qDebug() << "Initial selection point: " << init_sel_point;
    }

    void updateSelDragBox(QPointF next_point)
    {
        drag_box->setRect(QRectF(QPointF(std::min(next_point.x(), init_sel_point.x()), std::min(next_point.y(), init_sel_point.y())),
            QPointF(std::max(next_point.x(), init_sel_point.x()), std::max(next_point.y(), init_sel_point.y()))));
    }

    QRectF returnSelArea()
    {
        drag_box->hide();
        return drag_box->rect();
    }

           // Called when selection is initially made
    void drawSelBox(std::shared_ptr<QGraphicsScene> scene)
    {
        rect_item = scene->addRect(bounding_box,sel_box_pen);
        sel_NW_canvas = scene->addRect(sel_NW, sel_box_pen);
        sel_NE_canvas = scene->addRect(sel_NE, sel_box_pen);
        sel_SW_canvas = scene->addRect(sel_SW, sel_box_pen);
        sel_SE_canvas = scene->addRect(sel_SE, sel_box_pen);
        sel_N_canvas = scene->addRect(sel_N, sel_box_pen);
        sel_S_canvas = scene->addRect(sel_S, sel_box_pen);
        sel_E_canvas = scene->addRect(sel_E, sel_box_pen);
        sel_W_canvas = scene->addRect(sel_W, sel_box_pen);
        qDebug() << "drawn";
        updateSelBox();
    }

    void hideSelBox()
    {
        rect_item->hide();
        sel_NW_canvas->hide();
        sel_NE_canvas->hide();
        sel_SW_canvas->hide();
        sel_SE_canvas->hide();
        sel_N_canvas->hide();
        sel_S_canvas->hide();
        sel_E_canvas->hide();
        sel_W_canvas->hide();
    }

    void updateSelBox()
    {
        qDebug() << "Updating sel box bounds";
        // first, recalculate size of selection bounding box
        recalcSelBounds();
        midpoint_x = (min_x+max_x)/2;
        midpoint_y = (min_y+max_y)/2;

        qDebug() << "Updating selection box";
        // then, adjust bounding box dimensions
        bounding_box.setTopLeft(QPointF(min_x, min_y));
        bounding_box.setBottomRight(QPointF(max_x, max_y));
                                                            // then, reposition drag points
        sel_NW.moveTo(min_x-3, min_y-3);
        sel_NE.moveTo(max_x-3, min_y-3);
        sel_SW.moveTo(min_x-3, max_y-3);
        sel_SE.moveTo(max_x-3, max_y-3);
        sel_N.moveTo(midpoint_x, min_y-3);
        sel_E.moveTo(max_x-3, midpoint_y);
        sel_S.moveTo(midpoint_x, max_y-3);
        sel_W.moveTo(min_x-3, midpoint_y);

               // set changes to elements on canvas
        rect_item->setRect(bounding_box);
        sel_NW_canvas->setRect(sel_NW);
        sel_NE_canvas->setRect(sel_NE);
        sel_SW_canvas->setRect(sel_SW);
        sel_SE_canvas->setRect(sel_SE);
        sel_N_canvas->setRect(sel_N);
        sel_E_canvas->setRect(sel_E);
        sel_S_canvas->setRect(sel_S);
        sel_W_canvas->setRect(sel_W);
        qDebug() << "Updated to scene";
    }

           // recalculates bounds of selection box, should be called after every transformation
    void recalcSelBounds()
    {
        this->max_x = 0;
        this->max_y = 0;
        this->min_x = INFINITY;
        this->min_y = INFINITY;

        qDebug() << "Recalculating sel box bounds";
        if(sel_list.size() == 0){
            this->min_y = 0;
            this->min_x = 0;
            this->max_y = 0;
            this->max_x = 0;
            hideSelBox();
        }
        else
        {
            qDebug() << "not empty";
            for (std::list<uint64_t>::iterator it=sel_list.begin(); it != sel_list.end(); ++it)
            {
                uint64_t obj_id = *it;
                qDebug() << "got id";
                state.manipulatePage(page_id, [obj_id, this](Page& page)
                    {
                        qDebug() << "getting bounds for " << obj_id;
                        page.manipulateObject(obj_id, [this](CanvasObject &object){
                            qDebug() << "we are manipulating page now";
                            auto bound_rect = object.item->boundingRect();
                            qDebug() << "got bounding rect";
                            this->min_x = std::min(this->min_x, bound_rect.left());
                            this->max_x = std::max(this->max_x, bound_rect.right());
                            this->min_y = std::min(this->min_y, bound_rect.top());
                            this->max_y = std::max(this->max_y, bound_rect.bottom());
                            qDebug() << min_x;
                            qDebug() << min_y;
                            qDebug() << max_x;
                            qDebug() << max_y;
                        });
                    });
            }
        }
    }

    /* When a CanvasObject is selected, the midpoint of all CanvasObjects must be reevaluated when an object is added
     * so that it isn't calculated every time a transform happens.
     */
    void addSelection (uint64_t obj_id)
    {
        if (obj_id != -1)
            sel_list.push_back(obj_id);
    }

    void addSelection (std::list<uint64_t> obj_ids)
    {
        for (std::list<uint64_t>::iterator it=obj_ids.begin(); it != obj_ids.end(); ++it)
        {
            if (*it != -1)
                sel_list.push_back(*it);
        }
    }

           // remove everything from selection (or no longer selecting)
    void deleteSelection ()
    {
        for (std::list<uint64_t>::iterator it=sel_list.begin(); it != sel_list.end(); ++it)
        {
            uint64_t del_obj_id = *it;
            state.manipulatePage(page_id, [del_obj_id](Page& page)
                {
                    page.deleteObject(del_obj_id);
                });
        }
        sel_list.clear();
    }

           // remove individual objects from selection
    void removeSelection(uint64_t remove_id)
    {
        state.manipulatePage(page_id, [remove_id](Page& page)
                             {
                                 page.deleteObject(remove_id);
                             });
        sel_list.remove(remove_id);
    }

    void scaleSelection(enum::SelDragPoint dragPoint, double scale_factor_x, double scale_factor_y)
    {
        double scale_center_x;
        double scale_center_y;

               // there is probably a better way to do this
        switch(dragPoint)
        {
        case NW:
            scale_center_x = this->min_x;
            scale_center_y = this->min_y;
            break;
        case N:
            scale_center_x = this->midpoint_x;
            scale_center_y = this->min_y;
            break;
        case NE:
            scale_center_x = this->max_x;
            scale_center_y = this->min_y;
            break;
        case W:
            scale_center_x = this->min_x;
            scale_center_y = this->midpoint_y;
            break;
        case E:
            scale_center_x = this->max_x;
            scale_center_y = this->midpoint_y;
            break;
        case SW:
            scale_center_x = this->min_x;
            scale_center_y = this->max_y;
            break;
        case SE:
            scale_center_x = this->max_x;
            scale_center_y = this->max_y;
            break;
        case S:
            scale_center_x = this->midpoint_x;
            scale_center_y = this->max_y;
            break;
        }

        for (std::list<uint64_t>::iterator it=sel_list.begin(); it != sel_list.end(); ++it)
        {
            uint64_t scale_id = *it;
            state.manipulatePage(page_id, [scale_id, scale_center_x, scale_center_y, scale_factor_x, scale_factor_y, this](Page& page)
                {
                    page.manipulateObject(scale_id, [scale_id, scale_center_x, scale_center_y, scale_factor_x, scale_factor_y, this](CanvasObject &object){
                        nlohmann::json scale_json;
                        object.createScaleEvent(scale_json, scale_center_x,
                            scale_center_y, scale_factor_x,
                            scale_factor_y);
                    });
                });
        }
    }

    void rotateSelection(double degrees)
    {
        for (std::list<uint64_t>::iterator it=sel_list.begin(); it != sel_list.end(); ++it)
        {
            uint64_t rotate_id = *it;
            state.manipulatePage(page_id, [rotate_id, degrees, this](Page& page)
                {
                    page.manipulateObject(rotate_id, [this, degrees](CanvasObject &object){
                        nlohmann::json rotate_json;
                        object.createRotateEvent(rotate_json, this->midpoint_x, this->midpoint_y, degrees);
                    });
                });
        }
    }

    void moveSelection(double distance_x, double distance_y, ClientWebSocketHandler &ws_handler)
    {
        nlohmann::json event_json;
        qDebug() << "moving " << distance_x;
        qDebug() << "moving " << distance_y;
        for (std::list<uint64_t>::iterator it=sel_list.begin(); it != sel_list.end(); ++it)
        {
            bool was_real = false;
            uint64_t move_id = *it;
            state.manipulatePage(page_id, [&was_real, move_id, &event_json, distance_x, distance_y](Page& page)
            {
                page.manipulateObject(move_id, [&was_real, &event_json, distance_x, distance_y](CanvasObject &object)
                {
                    object.createMoveEvent(event_json, distance_x, distance_y);
                    was_real = true;
                });
            });
            if (!was_real) {
                continue;
            }
            qDebug() << "sending event";
            ws_handler.sendEvent(event_json);
            ws_handler.handleEvent(event_json);
            qDebug() << "event sent";
        }
    }
};

#ifndef SELECTION_HPP
#define SELECTION_HPP
#endif // SELECTION_HPP
