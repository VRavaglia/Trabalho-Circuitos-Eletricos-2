#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QCoreApplication>
#include <QFileDialog>
#include <QDebug>
#include <QThread>
#include <mna1.h>
#include <QMessageBox>
#include <QInputDialog>


void MainWindow::i_erro(QString msg_erro){
    QMessageBox msg_box;
    msg_box.setText("O seguinte erro ocorreu:/n/n"+msg_erro);
    msg_box.setStandardButtons(QMessageBox::Ok);
    msg_box.setDefaultButton(QMessageBox::Ok);
    msg_box.exec();
};

void MainWindow::i_printf(const char* format, ...){
    char buffer[1024];
    va_list argptr;
    va_start(argptr, format);
    vsnprintf(buffer, 1024, format, argptr);
    va_end(argptr);

    texto_console = texto_console + buffer;
    ui->console->setText(texto_console);
};

QString MainWindow::i_scanf(const char* format, ...){
    char buffer[1024];
    va_list argptr;
    va_start(argptr, format);
    vsnprintf(buffer, 1024, format, argptr);
    va_end(argptr);

    return openFile();
};

QString MainWindow::openFile()
{
    QString filename =  QFileDialog::getOpenFileName(
          this,
          "Selecione o arquivo de simulação:",
          QDir::currentPath(),
          "Netlist (*.net) ;; Todos os arquivos (*.*)");

    if( !filename.isNull() )
    {
      qDebug() << "selected file path : " << filename.toUtf8();
      return filename;
    }
    return "";
};


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_b_abrir_clicked()
{
    this->filename = openFile();

    ui->dir->setText(this->filename);
}

void MainWindow::on_pushButton_2_clicked()
{
    QByteArray ba = this->filename.toLocal8Bit();
    char *c_filename = ba.data();
    calculo(2, c_filename, *this);
}
