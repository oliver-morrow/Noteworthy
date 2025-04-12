#include "widget.h"
#include "AppNavigator.hpp"
#include "ui_widget.h"
#include "drawingroom.h"
#include <QLabel>
#include <QRegularExpression>
#include <QRegularExpressionValidator> // For QRegularExpressionValidator
#include "newuser.h"
#include <QKeyEvent>
#include <qtimer.h>




Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    // ui->lineEdit_5->setMaxLength(2);
    // ui->lineEdit_6->setMaxLength(2);
    // ui->lineEdit_7->setMaxLength(2);
    // ui->lineEdit_8->setMaxLength(2);
    ui->lineEdit_9->setMaxLength(1);

    QRegularExpression regex("[A-Za-z1-9]+");
    QValidator *validator = new QRegularExpressionValidator(regex, this);

    ui->lineEdit_5->setValidator(validator);
    ui->lineEdit_9->setValidator(validator);
    ui->lineEdit_7->setValidator(validator);
    ui->lineEdit_8->setValidator(validator);
    ui->lineEdit_9->setValidator(validator);

    ui->newpage->setCursor(Qt::PointingHandCursor);
    ui->codeEnter->setCursor(Qt::PointingHandCursor);
    ui->changename->setCursor(Qt::PointingHandCursor);
    // ui->getfile->setCursor(Qt::PointingHandCursor);

    lineEdits = {ui->lineEdit_5, ui->lineEdit_6, ui->lineEdit_7, ui->lineEdit_8, ui->lineEdit_9};

    // Install event filters on all line edits
    for (QLineEdit *lineEdit : lineEdits) {
        lineEdit->installEventFilter(this);
    }



    // QIcon fileicon(":/svg/svg/upload_file_24dp_FFFFFF_FILL0_wght400_GRAD0_opsz24.svg");
    // ui->getfile->setIcon(fileicon);
    // ui->getfile->setIconSize(QSize(120, 120));

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

void Widget::on_lineEdit_5_textChanged(const QString &arg1)
{
    setErrorText("");
    qDebug() << "Text in lineEdit_5 changed:" << arg1;

    if (ui->lineEdit_5->text().length() == 0) {
        ui->lineEdit_5->setFocus();  // Move back to previous line edit
        return;
    }

    if (arg1.length() == 1) {
        ui->lineEdit_5->setText(arg1.toUpper());
        ui->lineEdit_6->setFocus();
    }
    handleTextChange(ui->lineEdit_5, ui->lineEdit_6, arg1);
}


void Widget::on_lineEdit_6_textChanged(const QString &arg1)
{
    setErrorText("");

    if (arg1.isEmpty()) {
        ui->lineEdit_5->setFocus();  // Move back to previous line edit
        return;
    }

    if (arg1.length() == 1) {
        ui->lineEdit_6->setText(arg1.toUpper());
        ui->lineEdit_7->setFocus();
    }
    handleTextChange(ui->lineEdit_6, ui->lineEdit_7, arg1);
}


void Widget::on_lineEdit_7_textChanged(const QString &arg1)
{
    setErrorText("");

    if (arg1.isEmpty()) {
        ui->lineEdit_6->setFocus();  // Move back to previous line edit
        return;
    }

    if (arg1.length() == 1) {
        ui->lineEdit_7->setText(arg1.toUpper());
        ui->lineEdit_8->setFocus();
    }
    handleTextChange(ui->lineEdit_7, ui->lineEdit_8, arg1);
}


void Widget::on_lineEdit_8_textChanged(const QString &arg1)
{
    setErrorText("");

    if (ui->lineEdit_8->text().length() == 0) {
        ui->lineEdit_7->setFocus();  // Move back to previous line edit
        return;
    }

    if (arg1.length() == 1) {
        ui->lineEdit_8->setText(arg1.toUpper());
        ui->lineEdit_9->setFocus();
    }
    handleTextChange(ui->lineEdit_8, ui->lineEdit_9, arg1);
}


void Widget::on_lineEdit_9_textChanged(const QString &arg1)
{
    setErrorText("");

    if (arg1.isEmpty()) {
        ui->lineEdit_8->setFocus();  // Move back to previous line edit
        return;
    }

    if (arg1.length() == 1) {
        ui->lineEdit_9->setText(arg1.toUpper());
        ui->lineEdit_9->setFocus();
    }

}

void Widget::on_codeEnter_clicked()
{
    QString code = ui->lineEdit_5->text() +
                   ui->lineEdit_6->text() +
                   ui->lineEdit_7->text() +
                   ui->lineEdit_8->text() +
                   ui->lineEdit_9->text();

    QString name = ui->name->text();
    navigator->goToRoomPageJoin(code.toStdString(), username.toStdString());
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


bool Widget::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event); // Safely cast after including QKeyEvent
        if (keyEvent->key() == Qt::Key_Backspace) {
            for (int i = 0; i < lineEdits.size(); ++i) {
                if (watched == lineEdits[i] && lineEdits[i]->text().isEmpty() && i > 0) {
                    lineEdits[i - 1]->setFocus();
                    return true;
                }
            }
        }
    }

    // Pass unhandled events to the base class
    return QWidget::eventFilter(watched, event);
}






void Widget::on_lineEdit_9_returnPressed()
{
    QString code = ui->lineEdit_5->text() +
                   ui->lineEdit_6->text() +
                   ui->lineEdit_7->text() +
                   ui->lineEdit_8->text() +
                   ui->lineEdit_9->text();
    qDebug()  << code;
    QString name = ui->name->text();
    navigator->goToRoomPageJoin(code.toStdString(), username.toStdString());
}


void Widget::handleTextChange(QLineEdit *currentLineEdit, QLineEdit *nextLineEdit, const QString &arg1)
{
    if (arg1.length() == 2) {
        QString currentText = currentLineEdit->text();
        QString nextText = arg1.at(1);
        currentLineEdit->setText(currentText.at(0));
        nextLineEdit->setText(nextText);

        nextLineEdit->setFocus();
    }
}
