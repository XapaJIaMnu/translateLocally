#ifndef NETWORK_H
#define NETWORK_H
#include <QObject>
#include <QNetworkAccessManager>
#include <memory>

class QFile;

class Network : public QObject {
    Q_OBJECT
public:
    Network(QObject *parent);
    
    /**
     * Wrapper around QNetworkAccessManager::get that adds a User-Agent header.
     * Used by this class, but also useful on its own.
     */
    QNetworkReply *get(QNetworkRequest request);

    /**
     * Download a file to a destination on disk. Useful for skipping loading the
     * file in memory. The `downloadComplete(QFile,QString)` signal will be 
     * emitted with a pointer to the file and a suggested filename. During 
     * download the `progressBar(qint64,qint64)` signal will be emitted.
     */ 
    QNetworkReply *downloadFile(QUrl url, QFile* dest);

    /**
     * Overloaded version of `downloadFile(QUrl,QFile)` that downloads to a
     * `QTemporaryFile`. The file will be deleted when the returned 
     * `QNetworkReply` object is destroyed. But you can change this by changing
     * the file's parent.
     */
    QNetworkReply *downloadFile(QUrl url);
    
private:
    std::unique_ptr<QNetworkAccessManager> nam_;

signals:
    void downloadComplete(QFile* file, QString filename);
    void progressBar(qint64 ist, qint64 max);
    void error(QString);
};

#endif // NETWORK_H
