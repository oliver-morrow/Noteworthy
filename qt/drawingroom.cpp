#include "drawingroom.h"
#include "AppNavigator.hpp"
#include "RandomIdGenerator.hpp"
#include "RoomState.hpp"
#include "Shape.hpp"
#include "Symbol.hpp"
#include "load_file.hpp"
#include "ui_drawingroom.h"
#include <QApplication>
#include <QBuffer>
#include <QByteArray>
#include <QClipboard>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout> // For horizontal layout
#include <QIcon>
#include <QImage>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QSvgRenderer>
#include <QTabBar> // Add this line
#include <QTabWidget>
#include <QTimer>
#include <QTransform>

drawingRoom::drawingRoom(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::drawingRoom)
{
    ui->setupUi(this);

    ui->sidebartabs_2->setCurrentIndex(0);

    no_page_scene.setSceneRect(0, 0, 1000, 700);
    // Retrieve the default font set in main
    QFont customFont = QApplication::font();
    customFont.setPointSize(30); // Set font size

    // Create the text item
    QGraphicsTextItem* textItem = no_page_scene.addText("Add a page to start!", customFont);

    // Set the text color to rgba(74, 160, 60, 1)
    QColor textColor(74, 160, 60, 255); // RGBA with alpha fully opaque
    textItem->setDefaultTextColor(textColor);

    // Center the text item within the scene
    QRectF textRect = textItem->boundingRect();
    textItem->setPos(
        (no_page_scene.sceneRect().width() - textRect.width()) / 2, // Center horizontally
        (no_page_scene.sceneRect().height() - 100 - textRect.height()) / 2 // Center vertically
    );

    ui->graphics->setScene(&no_page_scene);

    connect(
        &ui->graphics->ws_handler,
        &ClientWebSocketHandler::pageCreated,
        this,
        &drawingRoom::handleCreatePageUIChange);

    connect(
        &ui->graphics->ws_handler,
        &ClientWebSocketHandler::pageDeleted,
        this,
        &drawingRoom::handleDeletePageUIChange);

    connect(
        &ui->graphics->ws_handler,
        &ClientWebSocketHandler::usersUpdated,
        this,
        &drawingRoom::handleUserUpdate);

    ui->collapse->setCursor(Qt::PointingHandCursor);
    ui->copyCode->setCursor(Qt::PointingHandCursor);

    // Initialize the timer for the cooldown
    collapseCooldownTimer = new QTimer(this);
    collapseCooldownTimer->setSingleShot(true); // Only triggers once per cooldown

    // Re-enable the collapse button when the timer expires
    connect(collapseCooldownTimer, &QTimer::timeout, this, [this]() { ui->collapse->setEnabled(true); });

    ui->sidebartabs_2->tabBar()->setVisible(false);

    ui->sidebartabs_2->setContentsMargins(0, 0, 0, 0);

    // settings button
    ui->settings->setMaximumSize(32, 32); // Set size to be a square
    ui->settings->setStyleSheet("QPushButton {"
                                "border-radius: 16px;"
                                "border: none;"
                                "background-color: transparent;"
                                "}");

    QIcon settingsicon(":/svg/svg/settings.svg"); // Path to your SVG file in the resources or filesystem
    ui->settings->setIcon(settingsicon);
    ui->settings->setIconSize(QSize(20, 20)); // Set icon size if needed

    // symbols button
    ui->symbols->setMaximumSize(32, 32); // Set size to be a square
    ui->symbols->setStyleSheet("QPushButton {"
                               "border-radius: 16px;"
                               "border: none;"
                               "background-color: transparent;"
                               "}");

    QIcon icon(":/svg/svg/interests.svg"); // Path to your SVG file in the resources or filesystem
    ui->symbols->setIcon(icon);
    ui->symbols->setIconSize(QSize(20, 20)); // Set icon size if needed

    // shapes button
    ui->shapes->setMaximumSize(32, 32); // Set size to be a square
    ui->shapes->setStyleSheet("QPushButton {"
                              "border-radius: 16px;"
                              "border: none;"
                              "background-color: transparent;"
                              "}");

    QIcon icon1(":/svg/svg/shapes.svg"); // Path to your SVG file in the resources or filesystem
    ui->shapes->setIcon(icon1);
    ui->shapes->setIconSize(QSize(20, 20)); // Set icon size if needed

    // pages button
    ui->pages->setMaximumSize(32, 32); // Set size to be a square
    ui->pages->setStyleSheet("QPushButton {"
                             "border-radius: 20px;"
                             "background-color: rgba(174, 243, 163, 1);"
                             "}");
    QIcon icon2(":/svg/svg/select_window_2.svg"); // Path to your SVG file in the resources or filesystem
    ui->pages->setIcon(icon2);
    ui->pages->setIconSize(QSize(20, 20)); // Set icon size if needed

    // collapse button
    ui->collapse->setMaximumSize(40, 40); // Set size to be a square
    ui->collapse->setStyleSheet("QPushButton {"

                                "}");

    QIcon icon3(":/svg/svg/menu_24dp_E8EAED_FILL0_wght400_GRAD0_opsz24.svg"); // Path to your SVG file in the resources or filesystem
    ui->collapse->setIcon(icon3);
    ui->collapse->setIconSize(QSize(30, 30)); // Set icon size if needed
    setButtonIconWithColor(ui->collapse, ":/svg/svg/menu_24dp_E8EAED_FILL0_wght400_GRAD0_opsz24.svg", QColor(74, 160, 60));

    // leave room button
    ui->leaveRoom->setMaximumSize(40, 40); // Set size to be a square
    ui->leaveRoom->setStyleSheet("QPushButton {"

                                 "}");

    QIcon icon5(":/svg/svg/arrow_back_ios_new.svg"); // Path to your SVG file in the resources or filesystem
    ui->leaveRoom->setIcon(icon5);
    ui->leaveRoom->setIconSize(QSize(19, 19)); // Set icon size if needed

    // select button
    ui->select->setMaximumSize(40, 40); // Set size to be a square
    ui->select->setStyleSheet("QPushButton {"
                              "background-color: rgba(174, 243, 163, 1);"
                              "padding-left: 10px; padding-right: 10px;"
                              "border-radius: 20px;" // Radius = half of the width/height
                              "}");
    QIcon icon6(":/svg/svg/select.svg"); // Path to your SVG file in the resources or filesystem
    ui->select->setIcon(icon6);
    ui->select->setIconSize(QSize(23, 23)); // Set icon size if needed

    // // save button
    // ui->save->setMaximumSize(40, 40); // Set size to be a square
    // ui->save->setStyleSheet("QPushButton {"
    //                         "background-color: rgba(174, 243, 163, 1);"
    //                         "border-radius: 20px;" // Radius = half of the width/height
    //                         "padding-left: 10px; padding-right: 10px;"
    //                         "}");
    // QIcon icon7(":/svg/svg/save.svg"); // Path to your SVG file in the resources or filesystem
    // ui->save->setIcon(icon7);
    // ui->save->setIconSize(QSize(23, 23)); // Set icon size if needed

    // // article button
    // ui->article->setMaximumSize(40, 40); // Set size to be a square
    // ui->article->setStyleSheet("QPushButton {"
    //                            "background-color: rgba(174, 243, 163, 1);"
    //                            "border-radius: 20px;" // Radius = half of the width/height
    //                            "padding-left: 10px; padding-right: 10px;"
    //                            "}");
    // QIcon icon8(":/svg/svg/article.svg"); // Path to your SVG file in the resources or filesystem
    // ui->article->setIcon(icon8);
    // ui->article->setIconSize(QSize(23, 23)); // Set icon size if needed

    // copy code button
    ui->copyCode->setMaximumSize(40, 40); // Set size to be a square
    ui->copyCode->setStyleSheet("QPushButton {"
                                "background-color: rgba(174, 243, 163, 1);"
                                "border-radius: 20px;"
                                "padding-left: 10px; padding-right: 10px;"
                                "}");
    QIcon icon9(":/svg/svg/content_copy.svg"); // Path to your SVG file in the resources or filesystem
    ui->copyCode->setIcon(icon9);
    ui->copyCode->setIconSize(QSize(23, 23)); // Set icon size if needed

    // draw button
    ui->draw->setMaximumSize(32, 32); // Set size to be a square
    ui->draw->setStyleSheet("QPushButton {"
                            "border-radius: 16px;"
                            "background-color: rgba(174, 243, 163, 1);"
                            "}");
    QIcon icon10(":/svg/svg/draw.svg"); // Path to your SVG file in the resources or filesystem
    ui->draw->setIcon(icon10);
    ui->draw->setIconSize(QSize(20, 20)); // Set icon size if needed

    // erase button
    ui->erase->setMaximumSize(32, 32); // Set size to be a square
    ui->erase->setStyleSheet("QPushButton {"
                             "border-radius: 16px;"
                             "border: none;"
                             "background-color: transparent;"
                             "}");
    QIcon icon11(":/svg/svg/ink_eraser.svg"); // Path to your SVG file in the resources or filesystem
    ui->erase->setIcon(icon11);
    ui->erase->setIconSize(QSize(20, 20)); // Set icon size if needed

    ui->thumbnailScrollableLayout->setAlignment(Qt::AlignTop);
    ui->scrollAreaWidgetContents->setLayout(ui->thumbnailScrollableLayout);
    // symbola icons
    QIcon icon12(":/png/png/capacitor.svg");
    ui->symbol1->setIcon(icon12);
    ui->symbol1->setIconSize(QSize(70, 140));

    QIcon icon13(":/png/png/resistor.svg"); // Path to your SVG file in the resources or filesystem
    ui->symbol2->setIcon(icon13);
    ui->symbol2->setIconSize(QSize(70, 140)); // Set icon size if needed

    QIcon icon14(":/png/png/inductor.svg"); // Path to your SVG file in the resources or filesystem
    ui->symbol3->setIcon(icon14);
    ui->symbol3->setIconSize(QSize(70, 140)); // Set icon size if needed

    QIcon icon15(":/png/png/battery.svg"); // Path to your SVG file in the resources or filesystem
    ui->symbol4->setIcon(icon15);
    ui->symbol4->setIconSize(QSize(70, 140)); // Set icon size if needed

    QIcon icon16(":/png/png/diode.svg"); // Path to your SVG file in the resources or filesystem
    ui->symbol5->setIcon(icon16);
    ui->symbol5->setIconSize(QSize(70, 140)); // Set icon size if needed

    QIcon icon17(":/png/png/switch.svg"); // Path to your SVG file in the resources or filesystem
    ui->symbol6->setIcon(icon17);
    ui->symbol6->setIconSize(QSize(70, 140)); // Set icon size if needed

    QIcon icon18(":/png/png/ac_source.svg"); // Path to your SVG file in the resources or filesystem
    ui->symbol7->setIcon(icon18);
    ui->symbol7->setIconSize(QSize(70, 140)); // Set icon size if needed

    QIcon icon19(":/png/png/dc_source.svg"); // Path to your SVG file in the resources or filesystem
    ui->symbol8->setIcon(icon19);
    ui->symbol8->setIconSize(QSize(70, 140)); // Set icon size if needed

    QIcon icon20(":/png/png/current_source"); // Path to your SVG file in the resources or filesystem
    ui->symbol9->setIcon(icon20);
    ui->symbol9->setIconSize(QSize(70, 140)); // Set icon size if needed

    QIcon icon21(":/png/png/square.svg");
    ui->shape1->setIcon(icon21);
    ui->shape1->setIconSize(QSize(50, 50));

    QIcon icon22(":/png/png/circle.svg");
    ui->shape2->setIcon(icon22);
    ui->shape2->setIconSize(QSize(50, 50));

    QIcon icon23(":/png/png/line.svg");
    ui->shape3->setIcon(icon23);
    ui->shape3->setIconSize(QSize(50, 50));

    QIcon icon24(":/png/png/ellipse.svg");
    ui->shape4->setIcon(icon24);
    ui->shape4->setIconSize(QSize(100, 50));

    QIcon icon25(":/png/png/rectangle.svg");
    ui->shape5->setIcon(icon25);
    ui->shape5->setIconSize(QSize(100, 50));

    QIcon icon26(":/png/png/arrow.svg");
    ui->shape6->setIcon(icon26);
    ui->shape6->setIconSize(QSize(100, 50));

    QIcon icon27(":/png/png/star.svg");
    ui->shape7->setIcon(icon27);
    ui->shape7->setIconSize(QSize(100, 50));

    QIcon icon28(":/png/png/2d_plot.svg");
    ui->shape8->setIcon(icon28);
    ui->shape8->setIconSize(QSize(100, 100));

    QIcon icon29(":/png/png/3d_plot.svg");
    ui->shape9->setIcon(icon29);
    ui->shape9->setIconSize(QSize(100, 100));

    QIcon icon31(":/png/png/add_circle.svg");
    ui->setting1->setIcon(icon31);
    ui->setting1->setIconSize(QSize(45, 45));

    QIcon icon32(":/png/png/upload_file.svg");
    ui->setting2->setIcon(icon32);
    ui->setting2->setIconSize(QSize(45, 45));

    QIcon icon30(":/png/png/delete.svg");
    ui->setting3->setIcon(icon30);
    ui->setting3->setIconSize(QSize(45, 45));
}
void drawingRoom::setNameLabel(const QString& name)
{
    ui->name->setText(name); // Set the label in Widget
}

drawingRoom::~drawingRoom()
{
    delete ui;
    thumbnailList.clear();
}

void drawingRoom::on_collapse_clicked()
{

    if (!ui->collapse->isEnabled())
        return; // Return if button is disabled

    ui->collapse->setEnabled(false); // Disable button for cooldown
    collapseCooldownTimer->start(300); // Start the 300ms cooldown timer

    QPropertyAnimation* animationMin = new QPropertyAnimation(ui->sidebar, "minimumWidth");
    QPropertyAnimation* animationMax = new QPropertyAnimation(ui->sidebar, "maximumWidth");

    animationMin->setDuration(300);
    animationMax->setDuration(300);

    static QPoint originalPos; // Store the original position for reparenting

    if (ui->sidebar->width() > 80) { // If sidebar is currently expanded
        lastWidth = ui->sidebar->width();
        animationMin->setStartValue(280);
        animationMin->setEndValue(80); // Collapse to 60 width
        animationMax->setStartValue(280);
        animationMax->setEndValue(80);

        originalPos = ui->collapse->mapToGlobal(QPoint(0, 0));

        animationMin->start(QAbstractAnimation::DeleteWhenStopped);
        animationMax->start(QAbstractAnimation::DeleteWhenStopped);
        // ui->collapse->setStyleSheet(
        //     "background-color: transparent"
        // );

        QTimer::singleShot(300, this, [this]() {
            ui->collapse->setParent( this);  // Reparent to the main window or a different parent
            ui->collapse->move(mapFromGlobal(originalPos));  // Move to original global position

            ui->collapse->show();  // Ensure it's visible
            // Hide all child widgets except the collapse button
            for (QObject *child : ui->sidebar->children()) {
                if (QWidget *widget = qobject_cast<QWidget *>(child)) {
                    if (child != ui->collapse) {
                        widget->setVisible(false); // Hide child widgets
                    }
                }
            } });
    } else { // If sidebar is currently collapsed
        // Set the collapse button's parent
        ui->collapse->setParent(ui->button_bar);

        // Get the horizontal layout from button_bar
        QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(ui->button_bar->layout());
        if (layout) {
            // Insert the collapse button at the front of the layout
            layout->insertWidget(0, ui->collapse); // Insert at index 0 to place it at the front
        }

        ui->collapse->show(); // Show the button

        animationMin->setStartValue(80);
        animationMin->setEndValue(280); // Expand to full width
        animationMax->setStartValue(80);
        animationMax->setEndValue(280);

        for (QObject* child : ui->sidebar->children()) {
            if (QWidget* widget = qobject_cast<QWidget*>(child)) {
                widget->setVisible(true); // Show children
            }
        }

        animationMin->start(QAbstractAnimation::DeleteWhenStopped);
        animationMax->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void drawingRoom::setCodeLabel(const QString& codeText)
{
    ui->code->setText(codeText); // `codeLabel` is the QLabel in `drawingRoom`
}

void drawingRoom::setUser(const QString& initial)
{
    ui->user1->setText(initial); // `codeLabel` is the QLabel in `drawingRoom`
}

void drawingRoom::setButtonIconWithColor(QPushButton* button, const QString& svgPath, const QColor& color)
{
    // Load the SVG renderer
    QSvgRenderer renderer(svgPath);

    // Set desired dimensions for the icon
    int iconDimension = 30;
    QSize iconSize(iconDimension, 40);

    // Create a square pixmap for rendering
    QPixmap pixmap(iconSize);
    pixmap.fill(Qt::transparent);

    // Create a QPainter to render the SVG onto the pixmap
    QPainter painter(&pixmap);
    renderer.render(&painter);
    painter.end();

    // Set color overlay with CompositionMode_SourceIn to colorize the icon
    QPainter colorPainter(&pixmap);
    colorPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    colorPainter.fillRect(pixmap.rect(), color); // Apply the specified color
    colorPainter.end();

    // Set the final colorized pixmap as the icon
    button->setIcon(QIcon(pixmap));
    button->setIconSize(iconSize); // Ensure icon fits the button size
}

void drawingRoom::on_leaveRoom_clicked()
{
    ui->graphics->ws_handler.closeWebSocket();
    navigator->goToHomepage(QString::fromStdString(user_id));
}

void drawingRoom::on_draw_clicked()
{
    ui->draw->setStyleSheet("QPushButton {"
                            "background-color: rgba(174, 243, 163, 1);"
                            "border-radius: 16px;"
                            "}");

    ui->erase->setStyleSheet("QPushButton {"
                             "background-color: transparent;"
                             "border-radius: 16px;"
                             "border: none;"
                             "}");

    if (!ui->graphics->selection.sel_list.empty()) {
        ui->graphics->selection.hideSelBox();
        ui->graphics->selection.sel_list.clear();
        ui->graphics->selection.selecting = false;
        ui->graphics->selection.drag_box->hide();
    }
    ui->graphics->touch_state.current_touch_action = TouchState::APPEND_STROKE;
}

void drawingRoom::on_erase_clicked()
{
    ui->erase->setStyleSheet("QPushButton {"
                             "background-color: rgba(174, 243, 163, 1);"
                             "border-radius: 16px;"
                             "}");

    ui->draw->setStyleSheet("QPushButton {"
                            "background-color: transparent;"
                            "border-radius: 16px;"
                            "border: none;"
                            "}");

    if (!ui->graphics->selection.sel_list.empty()) {
        ui->graphics->selection.hideSelBox();
        ui->graphics->selection.sel_list.clear();
        ui->graphics->selection.selecting = false;
        ui->graphics->selection.drag_box->hide();
    }
    ui->graphics->touch_state.current_touch_action = TouchState::ERASE_STROKE;
}

void drawingRoom::on_pages_clicked()
{
    ui->sidebartabs_2->setCurrentIndex(0);
    ui->pages->setStyleSheet("QPushButton {"
                             "background-color: rgba(174, 243, 163, 1);"
                             "border-radius: 20px;"
                             "}");

    ui->shapes->setStyleSheet("QPushButton {"
                              "background-color: transparent;"
                              "border-radius: 16px;"
                              "border: none;"
                              "}");

    ui->symbols->setStyleSheet("QPushButton {"
                               "background-color: transparent;"
                               "border-radius: 16px;"
                               "border: none;"
                               "}");

    ui->settings->setStyleSheet("QPushButton {"
                                "background-color: transparent;"
                                "border-radius: 16px;"
                                "border: none;"
                                "}");
}

void drawingRoom::on_shapes_clicked()
{
    ui->sidebartabs_2->setCurrentIndex(1);

    ui->shapes->setStyleSheet("QPushButton {"
                              "background-color: rgba(174, 243, 163, 1);"
                              "border-radius: 20px;"
                              "}");

    ui->pages->setStyleSheet("QPushButton {"
                             "background-color: transparent;"
                             "border-radius: 16px;"
                             "border: none;"
                             "}");

    ui->symbols->setStyleSheet("QPushButton {"
                               "background-color: transparent;"
                               "border-radius: 16px;"
                               "border: none;"
                               "}");

    ui->settings->setStyleSheet("QPushButton {"
                                "background-color: transparent;"
                                "border-radius: 16px;"
                                "border: none;"
                                "}");
}

void drawingRoom::on_symbols_clicked()
{
    ui->sidebartabs_2->setCurrentIndex(2);

    ui->symbols->setStyleSheet("QPushButton {"
                               "background-color: rgba(174, 243, 163, 1);"
                               "border-radius: 20px;"
                               "}");

    ui->pages->setStyleSheet("QPushButton {"
                             "background-color: transparent;"
                             "border-radius: 16px;"
                             "border: none;"
                             "}");

    ui->shapes->setStyleSheet("QPushButton {"
                              "background-color: transparent;"
                              "border-radius: 16px;"
                              "border: none;"
                              "}");

    ui->settings->setStyleSheet("QPushButton {"
                                "background-color: transparent;"
                                "border-radius: 16px;"
                                "border: none;"
                                "}");
}

void drawingRoom::on_settings_clicked()
{
    ui->sidebartabs_2->setCurrentIndex(3);

    ui->settings->setStyleSheet("QPushButton {"
                                "background-color: rgba(174, 243, 163, 1);"
                                "border-radius: 20px;"
                                "}");

    ui->pages->setStyleSheet("QPushButton {"
                             "background-color: transparent;"
                             "border-radius: 16px;"
                             "border: none;"
                             "}");

    ui->shapes->setStyleSheet("QPushButton {"
                              "background-color: transparent;"
                              "border-radius: 16px;"
                              "border: none;"
                              "}");

    ui->symbols->setStyleSheet("QPushButton {"
                               "background-color: transparent;"
                               "border-radius: 16px;"
                               "border: none;"
                               "}");
}

void clearLayout(QVBoxLayout* layout)
{
    if (!layout)
        return;

    // Iterate through all the items in the layout
    while (QLayoutItem* item = layout->takeAt(0)) {
        qDebug() << "DELETE THUMBNAIL!!!!!!!!!!!!!! <<<<<<";
        // If the item contains a widget, delete it
        if (QWidget* widget = item->widget()) {
            widget->deleteLater(); // Schedule the widget for deletion
        }
        // If the item contains a layout, clear it recursively
        else if (QLayout* childLayout = item->layout()) {
            clearLayout(static_cast<QVBoxLayout*>(childLayout));
        }
        // Delete the layout item
        delete item;
    }
}
void drawingRoom::initialize(std::string initial_room)
{
    ui->graphics->setScene(&no_page_scene);
    ui->graphics->user_id = user_id;
    ui->graphics->resetTransform();
    // clear layout
    qDebug() << "clear thumbnails";
    thumbnailList.clear();

    // auto layout = ui->thumbnailScrollableLayout->layout();
    // static_cast<QBoxLayout *>(layout)->insertWidget(index, new_thumbnail.get());
    clearLayout(ui->thumbnailScrollableLayout);

    nlohmann::json event_list = nlohmann::json::parse(initial_room);
    for (auto& event : event_list) {
        ui->graphics->ws_handler.handleEvent(event);
    }

    emit ui->graphics->ws_handler.startConnection(user_id, state.room_id);
    ui->code->setText(QString::fromStdString(state.room_id));
    setNameLabel(QString::fromStdString(state.owner_id + "'s Room"));

    // if we dont show
    if (user_id == state.owner_id) {
        ui->changePasswordLabel->show();
        ui->PasswordEdit->show();
        ui->PasswordSetButton->show();
        ui->KickButton->show();
        ui->KickUsernameEdit->show();
        ui->userKickLabel->show();
    } else {
        ui->changePasswordLabel->hide();
        ui->PasswordEdit->hide();
        ui->PasswordSetButton->hide();
        ui->KickButton->hide();
        ui->KickUsernameEdit->hide();
        ui->userKickLabel->hide();
    }

    if (state.room_id == "12345") {
        if (user_id == state.owner_id) {
            ui->setting1->show();
            ui->setting2->show();
            ui->setting3->show();
        } else {
            ui->setting1->hide();
            ui->setting2->hide();
            ui->setting3->hide();
        }
    } else {
        ui->setting1->show();
        ui->setting2->show();
        ui->setting3->show();
    }

    // go to the page view
    on_pages_clicked();

    uint64_t first_page_id;
    if (!state.getFirstPageId(first_page_id)) {
        ui->graphics->current_page_id = 0;
        return;
    }

    select_page(first_page_id);
}

void drawingRoom::select_page(uint64_t page_id)
{

    auto it = std::find_if(
        thumbnailList.begin(),
        thumbnailList.end(),
        [&page_id](const auto& pair) { return pair.first == page_id; });

    if (it == thumbnailList.end()) {
        navigator->goToRoomPageJoin(state.room_id, ui->graphics->user_id, state.password);
        return;
    }

    ui->graphics->current_page_id = it->first;

    ui->graphics->displayScene(state.getScene(it->first));

    for (auto& thumbnail : thumbnailList) {
        thumbnail.second->setStyleSheet("background-color: white; "
                                        "border: 3px solid rgba(174,243,163,1); "
                                        "border-radius: 15px");
    }
    it->second->setStyleSheet("background-color: white; "
                              "border: 3px solid rgba(74,160,60,1); "
                              "border-radius: 15px");
}

std::shared_ptr<ClickableGraphicsView> drawingRoom::createThumbnail(std::shared_ptr<QGraphicsScene> scene)
{
    std::shared_ptr<ClickableGraphicsView> new_thumbnail = std::make_shared<ClickableGraphicsView>();
    new_thumbnail->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    new_thumbnail->scale(0.2, 0.2);
    new_thumbnail->setScene(scene.get());

    new_thumbnail->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    new_thumbnail->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    new_thumbnail->setMinimumSize(0, 170);
    new_thumbnail->setMaximumSize(16777215, 170);
    new_thumbnail->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    new_thumbnail->setStyleSheet("background-color: white; "
                                 "border: 3px solid rgba(174,243,163,1); "
                                 "border-radius: 15px");
    return new_thumbnail;
}

void drawingRoom::handleCreatePageUIChange(uint64_t page_id)
{
    // nothing for now
    // assuming you have a list of thumbnails in the drawing room
    // add it to the list a the specified position
    // std::list<std::pair<uint64_t, std::shared_ptr<ClickableGraphicsView>>> page_list;
    // iterate over all the pages ids until you find prev page id
    // then std::list::insert the page at that position
    // add the clickable graphics view to the scrollable list view
    qDebug() << "handlingCreatePageUIChange" << page_id;

    // get the page
    state.manipulatePage(page_id, [this, page_id](Page& page) {
        auto new_thumbnail = createThumbnail(page.scene);

        uint64_t prev_page_id;
        size_t index = 0;

        if (!state.getPrevPageId(page_id, prev_page_id)) {
            prev_page_id = 0;
        }

        if (prev_page_id == 0) {
            thumbnailList.push_front({ page_id, new_thumbnail });
        } else {
            auto it = std::find_if(
                thumbnailList.begin(),
                thumbnailList.end(),
                [&prev_page_id](const auto& pair) { return pair.first == prev_page_id; });

            if (it == thumbnailList.end()) {
                throw "page not found!";
            }

            index = std::distance(thumbnailList.begin(), std::next(it));
            thumbnailList.insert(std::next(it), { page_id, new_thumbnail });
        }

        auto layout = ui->thumbnailScrollableLayout->layout();
        static_cast<QBoxLayout*>(layout)->insertWidget(index, new_thumbnail.get());

        connect(new_thumbnail.get(), &ClickableGraphicsView::clicked, this, [this, page_id]() { select_page(page_id); });

        if (this->ui->graphics->current_page_id == 0) {
            select_page(page_id);
        }
    });
};
void drawingRoom::handleDeletePageUIChange(uint64_t page_id)
{
    // iterate over list of std::list<std::pair<uint64_t, std::shared_ptr<ClickableGraphicsView>>>
    // until you find the correct page
    // remove it from the qt scrollable view
    // remove it from this list
    if (ui->graphics->current_page_id == 0) {
        return;
    }

    // Find the thumbnail in the list
    auto it = std::find_if(
        thumbnailList.begin(),
        thumbnailList.end(),
        [&page_id](const auto& pair) { return pair.first == page_id; });

    if (it == thumbnailList.end()) {
        throw "thumbnail to be deleted doesn't exist!!";
    }

    // Remove the widget from the UI and delete it
    auto layout = ui->thumbnailScrollableLayout->layout();
    layout->removeWidget(it->second.get());

    it->second.reset(); // Explicitly reset the shared_ptr

    // Erase the thumbnail from the list
    thumbnailList.erase(it);

    // Determine which page to navigate to next
    uint64_t current_page_id = ui->graphics->current_page_id;
    uint64_t page_id_to_go_to = 0;
    uint64_t prev_page_id, next_page_id;
    bool go_to_empty_page = true;

    if (state.getPrevPageId(current_page_id, prev_page_id)) {
        page_id_to_go_to = prev_page_id;
        go_to_empty_page = false;
    } else if (state.getNextPageId(current_page_id, next_page_id)) {
        page_id_to_go_to = next_page_id;
        go_to_empty_page = false;
    }

    // Handle the empty page scenario
    if (go_to_empty_page) {
        ui->graphics->current_page_id = 0;
        ui->graphics->setScene(&no_page_scene);
        return;
    }

    select_page(page_id_to_go_to);
};

// void drawingRoom::on_output_room_clicked()
// {
//     nlohmann::json json;

//     state.toJsonEventList(json);

//     std::string output = json.dump();
// }

// void drawingRoom::on_exit_clicked()
// {
//     navigator->goToHomepage();
// }

// void drawingRoom::on_previous_page_clicked()
// {
//     uint64_t prev_page_id;
//     if (!state.getPrevPageId(ui->graphics->current_page_id, prev_page_id))
//     {
//         return;
//     }
//     state.manipulatePage(prev_page_id, [this](Page &page)
//                          { ui->graphics->displayScene(page.scene); });
//     ui->graphics->current_page_id = prev_page_id;
// }

// void drawingRoom::on_next_page_clicked()
// {
//     // find the current page in the list
//     uint64_t next_page_id;
//     if (!state.getNextPageId(ui->graphics->current_page_id, next_page_id))
//     {
//         return;
//     }
//     state.manipulatePage(next_page_id, [this](Page &page)
//                          { ui->graphics->displayScene(page.scene); });
//     ui->graphics->current_page_id = next_page_id;
// }

void drawingRoom::showErrorMessage(const QString& message)
{
    // Check if the errorTextItem already exists
    if (!errorTextItem) {
        // Create a new error text item
        errorTextItem = no_page_scene.addText(message);
        errorTextItem->setDefaultTextColor(Qt::red); // Set text color to red
        QFont font = QApplication::font();
        font.setPointSize(24);
        font.setBold(true);
        errorTextItem->setFont(font);
    } else {
        // Update the message if the text item already exists
        errorTextItem->setPlainText(message);
        errorTextItem->show(); // Ensure the message is visible
    }

    // Recalculate the bounding rectangle to center the text properly
    QRectF textRect = errorTextItem->boundingRect();
    QRectF sceneRect = no_page_scene.sceneRect();

    errorTextItem->setPos(
        (sceneRect.width() - textRect.width()) / 2, // Center horizontally
        (sceneRect.height() / 2) + 70 // Adjust vertical position (below "Add a page to start!")
    );

    // Use a QTimer to hide the error message after 3 seconds
    QTimer::singleShot(3000, this, [this]() {
        if (errorTextItem) {
            errorTextItem->hide();
        }
    });
}

void drawingRoom::createShape(Shape::ShapeType type)
{

    if (ui->graphics->current_page_id == 0) {
        showErrorMessage("Error: No active page.");
        return;
    }

    Shape shape = Shape(type);
    uint64_t new_id = IDGenerator::newID();
    shape.object_id = new_id;
    shape.room_id = state.room_id;
    shape.page_id = ui->graphics->current_page_id;
    shape.owner_id = ui->graphics->user_id;

    std::random_device rd; // Seed for random number generator
    std::mt19937 gen(rd()); // Mersenne Twister random number generator

    std::uniform_int_distribution<> dist(0, 300);

    double randomNumber1 = dist(gen) * 1.5;
    double randomNumber2 = dist(gen);

    shape.top_left = { randomNumber1, randomNumber2 };

    QSvgRenderer renderer(Shape::shapeSvgPaths.at(type));
    if (!renderer.isValid()) {
        qDebug() << "SVG file is invalid for type:" << type;
        return;
    }

    QSizeF dimensions = renderer.defaultSize(); // Get default size of the SVG
    shape.bottom_left = {
        shape.top_left[0],
        shape.top_left[1] + dimensions.height()
    };
    shape.bottom_right = {
        shape.top_left[0] + dimensions.width(),
        shape.top_left[1] + dimensions.height()
    };

    nlohmann::json event;
    shape.createCreateEvent(event);

    ui->graphics->ws_handler.sendEvent(event);
    ui->graphics->ws_handler.handleEvent(event);
}

// add shape fucntionality once done
void drawingRoom::on_shape1_clicked()
{
    createShape(Shape::SQUARE);
}

void drawingRoom::on_shape2_clicked()
{
    createShape(Shape::CIRCLE);
}

void drawingRoom::on_shape3_clicked()
{
    createShape(Shape::LINE);
}

void drawingRoom::on_shape4_clicked()
{
    createShape(Shape::ELLIPSE);
}

void drawingRoom::on_shape5_clicked()
{
    createShape(Shape::RECTANGLE);
}

void drawingRoom::on_shape6_clicked()
{
    createShape(Shape::ARROW);
}

void drawingRoom::on_shape7_clicked()
{
    createShape(Shape::STAR);
}

void drawingRoom::on_shape8_clicked()
{
    createShape(Shape::TWOD_PLOT);
}

void drawingRoom::on_shape9_clicked()
{
    createShape(Shape::THREED_PLOT);
}

void drawingRoom::createSymbol(Symbol::SymbolType type)
{

    if (ui->graphics->current_page_id == 0) {
        showErrorMessage("Error: No active page.");
        return;
    }

    Symbol symbol = Symbol(type);
    uint64_t new_id = IDGenerator::newID();
    symbol.object_id = new_id;
    symbol.room_id = state.room_id;
    symbol.page_id = ui->graphics->current_page_id;
    symbol.owner_id = ui->graphics->user_id;

    std::random_device rd; // Seed for random number generator
    std::mt19937 gen(rd()); // Mersenne Twister random number generator

    std::uniform_int_distribution<> dist(0, 300);

    // Generate two random numbers
    double randomNumber1 = dist(gen) * 1.5;
    double randomNumber2 = dist(gen);

    symbol.top_left = { randomNumber1, randomNumber2 };
    QSvgRenderer renderer(Symbol::symbolSvgPaths.at(type));
    if (!renderer.isValid()) {
        qDebug() << "SVG file is invalid for type:" << type;
        return;
    }
    QSizeF dimensions = renderer.defaultSize(); // Get default size of the SVG
    symbol.bottom_left = {
        symbol.top_left[0],
        symbol.top_left[1] + dimensions.height()
    };
    symbol.bottom_right = {
        symbol.top_left[0] + dimensions.width(),
        symbol.top_left[1] + dimensions.height()
    };

    nlohmann::json event;
    symbol.createCreateEvent(event);

    ui->graphics->ws_handler.sendEvent(event);
    ui->graphics->ws_handler.handleEvent(event);
}

// add code once symbols are implemented
void drawingRoom::on_symbol1_clicked()
{
    createSymbol(Symbol::CAPACITOR);
}

void drawingRoom::on_symbol2_clicked()
{
    createSymbol(Symbol::RESISTOR);
}

void drawingRoom::on_symbol3_clicked()
{
    createSymbol(Symbol::INDUCTOR);
}

void drawingRoom::on_symbol4_clicked()
{
    createSymbol(Symbol::BATTERY);
}

void drawingRoom::on_symbol5_clicked()
{
    createSymbol(Symbol::DIODE);
}

void drawingRoom::on_symbol6_clicked()
{
    createSymbol(Symbol::SWITCH);
}

void drawingRoom::on_symbol7_clicked()
{
    createSymbol(Symbol::CURRENT_SOURCE);
}

void drawingRoom::on_symbol8_clicked()
{
    createSymbol(Symbol::DC_SOURCE);
}

void drawingRoom::on_symbol9_clicked()
{
    createSymbol(Symbol::CURRENT_SOURCE);
}

void drawingRoom::on_copyCode_clicked()
{
    QString codeText = ui->code->text();

    // Get the clipboard and set the text
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(codeText);
}

void drawingRoom::on_setting1_clicked()
{
    nlohmann::json json;
    uint64_t new_id = IDGenerator::newID();
    Page page;
    page.page_id = new_id;
    page.room_id = state.room_id;
    page.createInsertPageEvent(json, ui->graphics->current_page_id);
    ui->graphics->ws_handler.sendEvent(json);
    ui->graphics->ws_handler.handleEvent(json);
    state.manipulatePage(new_id, [this](Page& page) { ui->graphics->displayScene(page.scene); });
    ui->graphics->current_page_id = new_id;
    select_page(new_id);
}

void drawingRoom::on_setting3_clicked()
{
    // if after deleting page there are no pages left set scene to temporary scene
    uint64_t current_page_id = ui->graphics->current_page_id;

    if (current_page_id == 0) {
        return;
    }

    nlohmann::json json;
    state.manipulatePage(current_page_id, [&json](Page& page) {
        page.createDeleteEvent(json);
    });
    ui->graphics->ws_handler.sendEvent(json);
    ui->graphics->ws_handler.handleEvent(json);
}

void drawingRoom::on_setting2_clicked()
{
#ifdef __EMSCRIPTEN__ // if its webassembly
    open_file_and_process(state.room_id.c_str(), std::to_string(ui->graphics->current_page_id).c_str(), NW_HTTP.c_str(), state.password.c_str());
#endif

    qDebug() << "function jover";
}

void drawingRoom::on_select_clicked()
{
    ui->graphics->touch_state.current_touch_action = TouchState::DRAG_SELECTION;
}

void drawingRoom::handleUserUpdate() {

    std::vector<std::string> connected_users;
    std::vector<std::string> disconnected_users;

    state.forEachUser([&](const User &user){
        if (user.is_connected) {
            connected_users.push_back(user.username);
        } else {
            disconnected_users.push_back(user.username);
        }
    });

    std::string output_string = "Connected Users:\n";
    for (auto &username : connected_users) {
        output_string += username + "\n";
    }
    output_string += "\nDisconnected Users:\n";
    for (auto &username : disconnected_users) {
        output_string += username + "\n";
    }

    ui->usersTextBrowser->setText(QString::fromStdString(output_string));
}


void drawingRoom::on_KickButton_clicked()
{
    nlohmann::json event;
    if (!state.isUserConnectedToRoom(ui->KickUsernameEdit->text().toStdString())) {
        return;
    }

    state.manipulateUser(ui->KickUsernameEdit->text().toStdString(), [&](User& user) {
        user.createKickEvent(event);
    });
    ui->graphics->ws_handler.handleEvent(event);
    ui->graphics->ws_handler.sendEvent(event);
}

void drawingRoom::on_PasswordSetButton_clicked()
{
    nlohmann::json event;
    state.createChangePasswordEvent(event, ui->PasswordEdit->text().toStdString());
    ui->graphics->ws_handler.handleEvent(event);
    ui->graphics->ws_handler.sendEvent(event);
}

