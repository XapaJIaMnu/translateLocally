#include "Network.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtGlobal>
#include <iostream>
#include <mainwindow.h>

Network::Network(QObject *parent)
    : QObject(parent)
    , nam_(std::make_unique<QNetworkAccessManager>(this)) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    nam_->setTransferTimeout(10000); // Timeout the connection after 10 seconds
#endif
}

QNetworkReply* Network::downloadFile(const QString& urlstr) {
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
    return reply;
}

void  Network::downloadJson(const QString& urlstr) {
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
