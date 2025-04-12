#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QLineEdit>


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
    QList<QLineEdit *> lineEdits;  // List of line edits in correct order

    bool eventFilter(QObject *obj, QEvent *event) override;



private slots:
    void on_lineEdit_5_textChanged(const QString &arg1);

    void on_lineEdit_6_textChanged(const QString &arg1);

    void on_lineEdit_7_textChanged(const QString &arg1);

    void on_lineEdit_8_textChanged(const QString &arg1);

    void on_lineEdit_9_textChanged(const QString &arg1);

    void on_codeEnter_clicked();

    void on_changename_clicked();

    void on_getfile_clicked();

    void on_newpage_clicked();



    void on_lineEdit_9_returnPressed();
    void handleTextChange(QLineEdit *currentLineEdit, QLineEdit *nextLineEdit, const QString &arg1);


private:
    Ui::Widget *ui;
};
#endif // WIDGET_H
