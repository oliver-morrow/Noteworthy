#include "newuser.h"
#include "AppNavigator.hpp"
#include "ui_newuser.h"
#include "widget.h"
#include <QMessageBox>  // Include for message box
#include <qtimer.h>



NewUser::NewUser(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::NewUser)
{
    ui->setupUi(this);
    ui->button->setCursor(Qt::PointingHandCursor);


}

NewUser::~NewUser()
{
    delete ui;
}


void NewUser::on_button_clicked()
{

    // check if the line edit is empty
    on_lineEdit_returnPressed();
}


void NewUser::on_lineEdit_returnPressed()
{
    static const QRegularExpression validInputRegex("^[a-zA-Z0-9]*$");
    QString input = ui->lineEdit->text();
    qDebug() << "epic";
    // check if the line edit is empty
    if (input.isEmpty()) {
        // show a message box prompting the user to enter text
        ui->error->setText("Error: The input cannot be empty.");
        ui->error->setVisible(true);
        QTimer::singleShot(3000, this, [this]() {
            ui->error->setText("");
        });
    }else if (input.length()> 30){
        ui->error->setText("Error: Input cannot exceed 30 characters.");
        ui->error->setVisible(true);
        QTimer::singleShot(3000, this, [this]() {
            ui->error->setText("");
        });
    }
    else if (!input.contains(validInputRegex)) {
        ui->error->setText("Error: Input can only contain alphanumeric characters with no spaces.");
        ui->error->setVisible(true);
        QTimer::singleShot(3000, this, [this]() {
            ui->error->setText("");
        });
    }
    else {
        QString text = ui->lineEdit->text();
        // if the line edit has text, open the second window
        navigator->goToHomepage(text);
    }
}


