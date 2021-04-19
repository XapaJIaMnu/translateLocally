#include "Network.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <iostream>
#include <mainwindow.h>

Network::Network(QObject *parent)
    : QObject(parent)
    , nam_(std::make_unique<QNetworkAccessManager>(this)) {}

void  Network::downloadFile(QString& urlstr) {
    auto processDownload = [&]() {
        QNetworkReply * reply = qobject_cast<QNetworkReply *>(sender());
        QString filename = reply->url().fileName();
        if (reply->error() == QNetworkReply::NoError) { // Success
            emit downloadComplete(filename, reply->readAll());
         } else {
            emit error(reply->errorString());
        } 
        reply->deleteLater();
    };

    QNetworkReply *reply = nam_->get(QNetworkRequest(QUrl(urlstr)));
    connect(reply, &QNetworkReply::downloadProgress, this, &Network::progressBar);
    connect(reply, &QNetworkReply::finished, this, processDownload);
}

void  Network::downloadJson(QString& urlstr) {
    auto processJson = [&]() {
        QNetworkReply * reply = qobject_cast<QNetworkReply *>(sender());
        if (reply->error() == QNetworkReply::NoError) { // Success
            QByteArray result = reply->readAll();
            QJsonDocument jsonResponse = QJsonDocument::fromJson(result);
            emit getJson(jsonResponse.object());
        } else {
            emit error(reply->errorString());
        }
        reply->deleteLater();
    };
    QUrl url(urlstr);
    QNetworkRequest req = QNetworkRequest(url);
    QNetworkReply * reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, processJson);
}
