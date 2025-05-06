#include "mainwindow.h"
#include <QApplication>
#include <windows.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Настраиваем информацию о приложении
    QApplication::setApplicationName("Firmware Updater");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("Your Company");
    QApplication::setOrganizationDomain("yourcompany.com");

    MainWindow w;
    w.show();
    return a.exec();
}
