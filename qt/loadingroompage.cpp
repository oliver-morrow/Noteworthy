#include "loadingroompage.h"
#include "ui_loadingroompage.h"
#include "AppNavigator.hpp"

LoadingRoomPage::LoadingRoomPage(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::LoadingRoomPage)
{
    ui->setupUi(this);


    QVBoxLayout *layout = new QVBoxLayout(ui->centralwidget);
    ui->centralwidget->setLayout(layout);
}

LoadingRoomPage::~LoadingRoomPage()
{
    delete ui;
}

void LoadingRoomPage::on_submit_clicked()
{
    std::string password = ui->password->text().toStdString();
    if (password == "") {
        return;
    }

    navigator->goToRoomPageJoin(room_id, user_id, password);
    clear_password();
}


void LoadingRoomPage::on_back_clicked()
{
    navigator->goToHomepage();
    clear_password();
}


void LoadingRoomPage::on_password_textChanged(const QString &arg1)
{
    ui->error->setText("");
}
void LoadingRoomPage::setNameLabel(const QString &text) {
    ui->name->setText(text);
}

void LoadingRoomPage::on_password_returnPressed()
{
    std::string password = ui->password->text().toStdString();
    if (password == "") {
        return;
    }

    navigator->goToRoomPageJoin(room_id, user_id, password);
    clear_password();
}

