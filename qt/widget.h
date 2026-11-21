#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

public:
    void setLabelText(const QString &text);

    void setUsername(const QString& username);
    QString username;

    void setErrorText(const QString& error);

private slots:
    void on_roomCode_returnPressed();
    void on_codeEnter_clicked();
    void on_changename_clicked();
    void on_getfile_clicked();
    void on_newpage_clicked();

private:
    Ui::Widget *ui;
};
#endif // WIDGET_H
