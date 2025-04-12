#include "AppNavigator.hpp"
#include <QApplication>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    int id = QFontDatabase::addApplicationFont(":/fonts/fonts/Montserrat-SemiBold.ttf");

    QString family = QFontDatabase::applicationFontFamilies(id).at(0);

    // Check if the font was loaded successfully
    if (id != -1) {
        // Set the custom font as the default font
        QFont defaultFont(family);
        QApplication::setFont(defaultFont);
    }

    navigator = new AppNavigator();
    navigator->goToWelcomePage();

    return a.exec();
}
