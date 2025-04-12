#ifndef LOADINGROOMPAGE_H
#define LOADINGROOMPAGE_H

#include "ui_loadingroompage.h"
#include <QMainWindow>

namespace Ui {
class LoadingRoomPage;
}

class LoadingRoomPage : public QMainWindow
{
    Q_OBJECT

public:
    explicit LoadingRoomPage(QWidget *parent = nullptr);
    ~LoadingRoomPage();
    std::string room_id;
    std::string user_id;
    void setNameLabel(const QString &name);
    void clear_password() {
        ui->password->clear();
    }

    void setErrorText(const QString& error) {
        ui->error->setText(error);
    }

private slots:

    void on_submit_clicked();

    void on_back_clicked();

    void on_password_textChanged(const QString &arg1);

    void on_password_returnPressed();

private:
    Ui::LoadingRoomPage *ui;
};

#endif // LOADINGROOMPAGE_H
