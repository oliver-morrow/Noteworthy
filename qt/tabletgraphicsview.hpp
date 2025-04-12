#pragma once

#include "ClientWebSocketHandler.hpp"
#include "Stroke.hpp"
#include "Selection.hpp"
#include <QGraphicsView>
#include <QWidget>
#include <qgraphicsscene.h>
#include <QPainterPath>
#include <QEvent>
#include <QTouchEvent>
#include <QGraphicsPathItem>
#include <qwebsocket.h>
#include "EventTypeEnums.hpp"
#include "TouchState.hpp"

class TabletGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    TabletGraphicsView(
        QWidget *parent = nullptr) : QGraphicsView(parent)
    {

        setAttribute(Qt::WA_AcceptTouchEvents); // Enable touch events
        setAttribute(Qt::WA_TabletTracking);    // Enable tablet/stylus events
        setMouseTracking(true);                 // Enable tracking mouse movement
        setFocusPolicy(Qt::StrongFocus); // Allows mouse and keyboard events
        setAttribute(Qt::WA_Hover);             // Enable hover events
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);


        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        // i dont know why I have to subtract 5
        this->setSceneRect(0, 0, 1000, 700);

        this->pen = QColor{255, 50, 50};

        this->show();
    };

public:
    ClientWebSocketHandler ws_handler;

    void displayScene(std::shared_ptr<QGraphicsScene> scene)
    {
        this->setScene(scene.get());
    }

    QPointF centerPoint;

    uint64_t current_page_id = 0;
    std::string user_id;

    TouchState touch_state;
    Selection selection;
private:
    void resizeEvent(QResizeEvent *event) override;
    void handleTouch(QPointF position, int id);
    void handleRelease(QPointF position, int id);
    void handleMove(QPointF position, int id);

    void updateViewFromTouchState();

    QPen pen;

    std::unique_ptr<Stroke> current_stroke;
    std::unique_ptr<QPainterPath> erase_path;
    uint64_t current_stroke_id;

protected:
    bool event(QEvent *event) override
    {
        switch (event->type())
        {
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        {
            // Handle touch events
            QTouchEvent *touchEvent = static_cast<QTouchEvent *>(event);
            for (const QTouchEvent::TouchPoint &point : touchEvent->points())
            {
                int id = point.id();            // ID of the touch point
                QPointF pos = point.position(); // Current position of the touch point
                Qt::TouchPointState pointState = static_cast<Qt::TouchPointState>(point.state());

                switch (pointState)
                {
                case Qt::TouchPointPressed:
                    handleTouch(pos, id);
                    break;
                case Qt::TouchPointMoved:
                    handleMove(pos, id);
                    break;
                case Qt::TouchPointReleased:
                    handleRelease(pos, id);
                    break;
                default:
                    break;
                }
            }
            return true; // Event has been handled
        }

        case QEvent::MouseButtonPress:
        case QEvent::MouseMove:
        case QEvent::MouseButtonRelease:
        {
            // Handle mouse events
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            QPointF pos = mouseEvent->position();
            int id = 0; // Use 0 for mouse since there's usually only one pointer

            switch (event->type())
            {
            case QEvent::MouseButtonPress:
                handleTouch(pos, id);
                break;
            case QEvent::MouseMove:
                handleMove(pos, id);
                break;
            case QEvent::MouseButtonRelease:
                handleRelease(pos, id);
                break;
            default:
                break;
            }
            return true; // Event has been handled
        }

        default:
            break;
        }

        // Pass the event to the base class if not handled
        return QGraphicsView::event(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        event->setAccepted(false);  // Indicate the event hasn't been fully handled
        QGraphicsView::event(event); // Pass to `event()` for further handling
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        event->setAccepted(false);  // Mark event as unhandled
        QGraphicsView::event(event); // Pass to `event()` for further handling
    }
};
