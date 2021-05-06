#ifndef MODELMANAGER_H
#define MODELMANAGER_H
#include <QSettings>
#include <QAbstractTableModel>
#include <QDir>
#include <QList>
#include <QFuture>

class QNetworkAccessManager;

struct LocalModel {
    QString path; // This is full path to the directory
    QString name;
    QString shortName;
    QString type;
};

Q_DECLARE_METATYPE(LocalModel)

struct RemoteModel {
    QString name;
    QString code;
    QString url;
};

Q_DECLARE_METATYPE(RemoteModel)

class ModelManager : public QAbstractTableModel {
        Q_OBJECT
public:
    ModelManager(QObject *parent);
    void loadSettings();
    void writeModel(QString filename, QByteArray data);

    QList<LocalModel> installedModels() const;
    QList<RemoteModel> availableModels() const; // remote - local

    virtual QVariant data(QModelIndex const &, int role = Qt::DisplayRole) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    virtual int columnCount(QModelIndex const & = QModelIndex()) const;
    virtual int rowCount(QModelIndex const & = QModelIndex()) const;

    static const int kColumnName;
    static const int kColumnShortName;
    static const int kColumnPathName;
    static const int kColumnType;
    static const int kLastColumn;

public slots:
    void fetchRemoteModels();
    
private:
    void startupLoad();
    void scanForModels(QString path);
    void extractTarGz(QString filename);
    LocalModel parseModelInfo(QString path);
    void parseRemoteModels(QJsonObject obj);

    QSettings qset_;
    QDir configDir_;

    QStringList archives_; // Only archive name, not full path
    QList<LocalModel> localModels_;
    QList<RemoteModel> remoteModels_;

    QNetworkAccessManager *nam_;

signals:
    void newModelAdded(int index);
    void fetchedRemoteModels();
    void error(QString);
};

#endif // MODELMANAGER_H
