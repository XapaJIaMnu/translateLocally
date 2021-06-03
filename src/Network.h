#ifndef NETWORK_H
#define NETWORK_H
#include <QObject>
#include <QNetworkAccessManager>

class QFile;

class Network : public QObject {
    Q_OBJECT
public:
    Network(QObject *parent);
    QNetworkReply *downloadFile(QUrl url);
    QNetworkReply *downloadFile(QUrl url, QFile* dest);
private:
    std::unique_ptr<QNetworkAccessManager> nam_;

signals:
    void downloadComplete(QFile* file, QString filename);
    void progressBar(qint64 ist, qint64 max);
    void error(QString);
};

#endif // NETWORK_H
