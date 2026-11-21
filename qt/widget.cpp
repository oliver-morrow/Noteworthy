#include "widget.h"
#include "AppNavigator.hpp"
#include "ui_widget.h"
#include "drawingroom.h"
#include <QLabel>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include "newuser.h"
#include <qtimer.h>




Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    ui->roomCode->setMaxLength(8);
    ui->roomCode->setValidator(new QRegularExpressionValidator(
        QRegularExpression("[A-Za-z0-9]{0,8}"), this));

    ui->newpage->setCursor(Qt::PointingHandCursor);
    ui->codeEnter->setCursor(Qt::PointingHandCursor);
    ui->changename->setCursor(Qt::PointingHandCursor);

    QIcon pageicon(":/svg/svg/add_24dp_FFFFFF_FILL0_wght400_GRAD0_opsz24.svg");
    ui->newpage->setIcon(pageicon);
    ui->newpage->setIconSize(QSize(120, 120));
}





Widget::~Widget()
{
    delete ui;
}

void Widget::setLabelText(const QString &text) {
    ui->name->setText(text);
}

void Widget::on_codeEnter_clicked()
{
    const auto code = ui->roomCode->text().toUpper();
    if (code.size() != 8 && code.size() != 5) {
        setErrorText("Enter a valid room code");
        return;
    }
    navigator->goToRoomPageJoin(code.toStdString(), username.toStdString());
}

void Widget::on_roomCode_returnPressed()
{
    on_codeEnter_clicked();
}

void Widget::on_changename_clicked()
{
    navigator->goToWelcomePage();
}

void Widget::on_getfile_clicked()
{
    throw "not implemented upload file yet";
}

void Widget::on_newpage_clicked()
{
    navigator->goToRoomPageCreate(username.toStdString());
}

void Widget::setUsername(const QString& username) {
    this->username = username;
    ui->name->setText(username);
}

void Widget::setErrorText(const QString& error) {
    ui->error->setText(error);
    QTimer::singleShot(3000, this, [this]() {
        ui->error->setText("");
    });
}
