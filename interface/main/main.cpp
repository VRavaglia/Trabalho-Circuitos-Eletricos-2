#include "mainwindow.h"
#include <QApplication>
#include <QDebug>



int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow mainWindow;
    mainWindow.setFixedSize(800,600);
    mainWindow.setWindowTitle("Trabalho CE2");
    mainWindow.show();
    return app.exec();
}
