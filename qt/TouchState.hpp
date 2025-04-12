#include <QElapsedTimer>
#include <QLineF>
#include <QDebug>
#include <map>
#include <list>

class TouchState {

public:
    enum TouchAction {
        APPEND_STROKE,
        ERASE_STROKE,
        DRAG_SELECTION,
        DRAG_HANDLE,
    };
    TouchAction current_touch_action = APPEND_STROKE;

    void print() {
        qDebug() << "TouchState:";
        for (int id : touch_priority) {
            QPointF point = touch_points.at(id).first;
            qDebug() << "ID: " << id << ", Location: ("
                     << point.x() << ", " << point.y() << ")";
        }
    }

    void updateTouch(QPointF point, int id) {
#ifdef __EMSCRIPTEN__ // if it's WebAssembly
        if (id == 0) {
            return;
        }
#endif
        if (touch_points.count(id) == 0) {
            touch_priority.push_back(id);
            touch_points[id].second.start(); // Start a timer for the new touch point
        }
        touch_points[id].first = point; // Update the position
        removeStaleTouches(1500);
    }

    void removeTouch(int id) {
#ifdef __EMSCRIPTEN__ // if it's WebAssembly
        if (id == 0) {
            return;
        }
#endif
        touch_priority.remove(id);
        touch_points.erase(id);
        if (touch_priority.size() < 2) {
            prev_pinch_distance = 0;
        }
    }

    void removeStaleTouches(qint64 maxAgeMs) {
        // Iterate through all touch points and remove stale ones
        for (auto it = touch_points.begin(); it != touch_points.end(); ) {
            if (it->second.second.elapsed() > maxAgeMs) {
                touch_priority.remove(it->first);
                it = touch_points.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool wasPinching = false;

    bool isPinching() {
        return touch_priority.size() >= 2;
    }

    double getPinchDistance() {
        if (touch_priority.size() < 2) {
            throw "doesn't work";
        }

        auto it = touch_priority.begin();
        int first = *it;
        ++it;
        int second = *it;
        QPointF first_point = touch_points[first].first;
        QPointF second_point = touch_points[second].first;

        QLineF line {first_point, second_point};

        return line.length();
    }

    double getZoomRatio() {
        double pinch_distance = getPinchDistance();
        if (prev_pinch_distance == 0) {
            prev_pinch_distance = pinch_distance;
            return 1;
        }
        double ratio = pinch_distance / prev_pinch_distance;

        prev_pinch_distance = pinch_distance;
        if (ratio > 3) {
            return 3;
        }
        if (ratio < 0.3) {
            return 0.3;
        }
        return ratio;
    }

    QPointF getPinchCenterPoint() {
        if (touch_priority.size() < 2) {
            throw "doesn't work";
        }

        auto it = touch_priority.begin();
        int first = *it;
        ++it;
        int second = *it;
        QPointF first_point = touch_points[first].first;
        QPointF second_point = touch_points[second].first;

        return {(first_point.rx() + second_point.rx()) / 2,
            (first_point.ry() + second_point.ry()) / 2};
    }

    void setPrevCenterPoint(QPointF point) {
        prev_pinch_center_point = point;
    }

    QPointF getRelativeCenter() {
        QPointF current_center = getPinchCenterPoint();
        QPointF relative_center = current_center - prev_pinch_center_point;
        prev_pinch_center_point = current_center;
        return relative_center;
    }

private:
    QPointF prev_pinch_center_point;
    double prev_pinch_distance = 0;

           // Map of touch ID to pair of QPointF (position) and QElapsedTimer (timestamp)
    std::map<int, std::pair<QPointF, QElapsedTimer>> touch_points;

           // List to maintain touch order priority
    std::list<int> touch_priority;
};
