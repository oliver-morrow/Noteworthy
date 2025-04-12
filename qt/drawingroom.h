#ifndef DRAWINGROOM_H
#define DRAWINGROOM_H

#include "ui_drawingroom.h"
#include <QWidget>
#include <QPushButton>
#include <QGraphicsScene>
#include <clickablegraphicsview.h>
#include "Symbol.hpp"
#include "Shape.hpp"

namespace Ui {
class drawingRoom;
}

class drawingRoom : public QWidget
{
    Q_OBJECT

public:
    explicit drawingRoom(QWidget *parent = nullptr);
    ~drawingRoom();
    void setNameLabel(const QString &name);
    void setCodeLabel(const QString &codeText);
    void setButtonIconWithColor(QPushButton *button, const QString &svgPath, const QColor &color);
    void setUser(const QString &initial);
    void createSymbol(Symbol::SymbolType type);
    void createShape(Shape::ShapeType type);
    std::string user_id;
    QGraphicsScene no_page_scene;

    void closeWebSocket() {
        ui->graphics->ws_handler.closeWebSocket();
    }

    std::list<std::pair<uint64_t, std::shared_ptr<ClickableGraphicsView>>> thumbnailList;
    std::shared_ptr<ClickableGraphicsView> createThumbnail(std::shared_ptr<QGraphicsScene> scene);

    void initialize(std::string initial_room = "[]");

    void handleCreatePageUIChange(uint64_t page_id);

    void handleDeletePageUIChange(uint64_t page_id);

    void handleUserUpdate();

    void select_page(uint64_t page_id);

    void on_output_room_clicked();

    void on_exit_clicked();



private slots:
    void on_collapse_clicked();

    void on_leaveRoom_clicked();

    void on_draw_clicked();

    void on_erase_clicked();

    void on_pages_clicked();

    void on_shapes_clicked();

    void on_symbols_clicked();

    void on_settings_clicked();

    void on_shape1_clicked();

    void on_shape2_clicked();

    void on_shape3_clicked();

    void on_shape4_clicked();

    void on_shape5_clicked();

    void on_shape6_clicked();

    void on_shape7_clicked();

    void on_shape8_clicked();

    void on_shape9_clicked();

    void on_symbol1_clicked();

    void on_symbol2_clicked();

    void on_symbol3_clicked();

    void on_symbol4_clicked();

    void on_symbol5_clicked();

    void on_symbol6_clicked();

    void on_symbol7_clicked();

    void on_symbol8_clicked();

    void on_symbol9_clicked();

    void on_copyCode_clicked();

    void on_setting1_clicked();

    void on_setting3_clicked();

    void on_setting2_clicked();

    void on_select_clicked();

    void on_KickButton_clicked();

    void on_PasswordSetButton_clicked();

private:
    Ui::drawingRoom *ui;
    int lastWidth = 0;
    QTimer *collapseCooldownTimer;
    void showErrorMessage(const QString &message);
    QGraphicsTextItem *errorTextItem = nullptr;

};

#endif // DRAWINGROOM_H
