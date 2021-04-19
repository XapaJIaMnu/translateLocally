#ifndef NETWORK_H
#define NETWORK_H
#include <QObject>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QProgressBar>
#include <memory>
#include <QComboBox>


class Network : public QObject {
    Q_OBJECT
public:
    Network(QObject *parent);
    void downloadFile(QString& urlstr);// const std::function<void (QString, QByteArray, QNetworkReply::NetworkError)>& callback);
    void downloadJson(QString& urlstr); // const std::function<void (void /*QJsonObject, QNetworkReply::NetworkError*/)> * callback)
private:
    std::unique_ptr<QNetworkAccessManager> nam_;

signals:
    void downloadComplete(QString filename, QByteArray data, QString err);
    void progressBar(qint64 ist, qint64 max);
    void getJson(QJsonObject obj, QString err);
};

#endif // NETWORK_H
