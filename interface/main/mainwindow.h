#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <qtextbrowser.h>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void i_printf(const char*, ...);
    void i_erro(QString);

private slots:
    void on_b_abrir_clicked();

private:
    Ui::MainWindow *ui;

    QString openFile();

    /*QPushButton *b_abrir;
    QPushButton *b_rodar;
    QLabel *label;
    QLabel *console;*/
    QString texto_console;
};

#endif // MAINWINDOW_H
