#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QCoreApplication>
#include <QFileDialog>
#include <QDebug>
#include <QThread>
//#include <mna1.h>
#include <QMessageBox>


void MainWindow::i_erro(QString msg_erro){
    QMessageBox msgBox;
    msgBox.setText("O seguinte erro ocorreu:/n/n"+msg_erro);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
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

QString MainWindow::openFile()
{
    QString filename =  QFileDialog::getOpenFileName(
          this,
          "Selecione o arquivo de simulação:",
          QDir::currentPath(),
          "All files (*.*) ;; Document files (*.doc *.rtf)");

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
    auto fileName = openFile();

    ui->dir->setText(fileName);
}
