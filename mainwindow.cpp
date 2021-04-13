#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <memory>
#include "marianinterface.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , translator_(std::make_unique<MarianInterface>())
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_translateButton_clicked()
{
    ui->outputBox->setText(translator_->translate(ui->inputBox->toPlainText()));
}
