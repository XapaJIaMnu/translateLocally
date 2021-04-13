#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <memory>
#include "marianinterface.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , translator_(std::make_unique<MarianInterface>())
    , nam_(new QNetworkAccessManager(this))
{
    ui->setupUi(this);
    connect(nam_, &QNetworkAccessManager::finished, this, &MainWindow::onResult);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_translateButton_clicked()
{
    ui->outputBox->setText(translator_->translate(ui->inputBox->toPlainText()));
}

/**
 * @brief MainWindow::on_modelDownload_clicked fetches the available models json from the Internet.
 */
void MainWindow::on_modelDownload_clicked()
{
    QUrl url("http://data.statmt.org/bergamot/models/models.json");
    QNetworkRequest req = QNetworkRequest(url);
    nam_->get(req);
}

/**
 * @brief MainWindow::onResult reads the json for available models
 * @param reply Reply returned from QNetworkAccess manager
 */
void MainWindow::onResult(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) { // Success
        QByteArray result = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(result);
        QJsonObject obj = jsonResponse.object();
        QJsonArray array = obj["models"].toArray();
        for (auto&& arrobj : array) {
            QString name = arrobj.toObject()["name"].toString();
            QString code = arrobj.toObject()["code"].toString();
            QString url = arrobj.toObject()["url"].toString();
            QString line = "Name: " + name + " code: " + code + " url " +  url + "\n";
            ui->outputBox->append(line);
        }
    } else {
        ui->outputBox->setText(reply->errorString());
    }
    reply->deleteLater();
}
