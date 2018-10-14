#include "mainwindow.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QDebug>


QString filename = "";

class Arquivos : public QWidget
{
public:
  QString openFile()
  {
    QString filename =  QFileDialog::getOpenFileName(
          this,
          "Open Document",
          QDir::currentPath(),
          "All files (*.*) ;; Document files (*.doc *.rtf);; PNG files (*.png)");

    if( !filename.isNull() )
    {
      qDebug() << "selected file path : " << filename.toUtf8();
      return filename;
    }
    return "";
  }
};

MainWindow::MainWindow(QWidget *parent)
   : QMainWindow(parent)
{
   // Create the button, make "this" the parent
   m_button = new QPushButton("Abrir", this);
   // set size and location of the button
   m_button->setGeometry(QRect(QPoint(10, 10),
   QSize(80, 30)));



   // Connect button signal to appropriate slot
   connect(m_button, SIGNAL (released()), this, SLOT (handleButton()));


   label = new QLabel(this);
   label->setText("Arquivo selecionado: " + filename);
   label->setAlignment(Qt::AlignCenter);
   label->setGeometry(QRect(QPoint(0, 300),
                            QSize(800, 30)));

}

void MainWindow::handleButton()
{
    Arquivos arquivos;
    filename = arquivos.openFile();
    label->setText("Arquivo selecionado:" + filename);
}
