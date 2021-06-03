#include "Network.h"
#include <QNetworkReply>
#include <QTemporaryFile>

Network::Network(QObject *parent)
    : QObject(parent)
    , nam_(std::make_unique<QNetworkAccessManager>(this)) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    nam_->setTransferTimeout(10000); // Timeout the connection after 10 seconds
#endif
}

QNetworkReply* Network::downloadFile(QUrl url, QFile *dest) {
    QNetworkReply *reply = nam_->get(QNetworkRequest(url));
    
    // Open in read/write so we can easily read the data when handling the
    // downloadComplete signal.
    if (!dest->open(QIODevice::ReadWrite)) {
        emit error(tr("Cannot open file for downloading."));
        return nullptr;
    }
    
    // While chunks come in, write them to the temp file
    connect(reply, &QIODevice::readyRead, [=] {
        if (dest->write(reply->readAll()) == -1)
            emit error(tr("An error occurred while writing the downloaded data to disk: %1").arg(dest->errorString()));
    });

    // When finished, emit downloadComplete(QFile*,QString)
    connect(reply, &QNetworkReply::finished, [=] {
        if (reply->error() == QNetworkReply::NoError) { // Success
            dest->flush(); // Flush the last downloaded data
            dest->seek(0); // Rewind the file
            QString filename = reply->url().fileName();
            emit downloadComplete(dest, filename);
        } else {
            emit error(reply->errorString());
        }
        reply->deleteLater();
    });
    
    connect(reply, &QNetworkReply::downloadProgress, this, &Network::progressBar);

    return reply;
}

/**
 * Overloaded version of downloadFile that downloads to temporary file. If you
 * do not change the parent of the QTemporaryFile, it will be deleted
 * automatically after the downloadComplete(QFile*,QString) signal is handled.
 */
QNetworkReply* Network::downloadFile(QUrl url) {
    QTemporaryFile *dest = new QTemporaryFile();
    QNetworkReply *reply = downloadFile(url, dest);
    // Make the lifetime of the temporary as long as the reply object itself
    if (reply != nullptr)
        dest->setParent(reply);
    return reply;
}
