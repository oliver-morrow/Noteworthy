#ifndef CLICKABLEGRAPHICSVIEW_H
#define CLICKABLEGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QMouseEvent>

class ClickableGraphicsView : public QGraphicsView {
    Q_OBJECT

public:
    explicit ClickableGraphicsView(QWidget *parent = nullptr) : QGraphicsView(parent) {}

signals:
    void clicked(QPointF position);

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            // Map the click position to the scene coordinates
            QPointF scenePos = mapToScene(event->pos());
            emit clicked(scenePos); // Emit a signal with the position
        }
        // Call base class implementation to ensure default behavior
        QGraphicsView::mousePressEvent(event);
    }
};

#endif // CLICKABLEGRAPHICSVIEW_H
