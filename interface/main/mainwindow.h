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
    QString i_scanf(const char*, ...);

private slots:
    void on_b_abrir_clicked();

    void on_pushButton_2_clicked();

private:
    Ui::MainWindow *ui;

    QString openFile();

    /*QPushButton *b_abrir;
    QPushButton *b_rodar;
    QLabel *label;
    QLabel *console;*/
    QString texto_console;
    QString filename;
};

#endif // MAINWINDOW_H
