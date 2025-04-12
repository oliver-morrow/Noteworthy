#ifndef NEWUSER_H
#define NEWUSER_H

#include <QWidget>

namespace Ui {
class NewUser;
}

class NewUser : public QWidget
{
    Q_OBJECT

public:
    explicit NewUser(QWidget *parent = nullptr);
    ~NewUser();

private slots:
    void on_button_clicked();

    void on_lineEdit_returnPressed();

private:
    Ui::NewUser *ui;
};

#endif // NEWUSER_H
