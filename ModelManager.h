#ifndef MODELMANAGER_H
#define MODELMANAGER_H
#include <QSettings>
#include <QAbstractTableModel>
#include <QDir>
#include <QList>
#include <QFuture>
#include <iostream>
#include "types.h"

class QNetworkAccessManager;

struct RemoteModel {
    QString name;
    QString code;
    QString url;
};

Q_DECLARE_METATYPE(RemoteModel)

struct Model {
    QString shortName; // Unique model identifier eg en-es-tiny
    QString modelName; // Long name, to be displayed in a single line
    QString url = "";
    QString path = ""; // This is full path to the directory
    QString src;
    QString trg;
    QString type; // Base or tiny
    float localversion  = -1.0f;
    float localAPI = -1.0f;
    float remoteversion = -1.0f;
    float remoteAPI = -1.0f;

    void set(QString key, QString val) {
        if (key == "shortName") {
            shortName = key;
        } else if (key == "modelName") {
            modelName = val;
        } else if (key == "url") {
            url = val;
        } else if (key == "path") {
            path = val;
        } else if (key == "src") {
            src = val;
        } else if (key == "trg") {
            trg = val;
        } else if (key == "type") {
            type = val;
        } else {
            std::cerr << "Unknown key type. " << key.toStdString() << " Something is very wrong!" << std::endl;
        }
    }
    void set(QString key, float val) {
        if (key == "localversion") {
            localversion = val;
        } else if (key == "localAPI") {
            localAPI = val;
        } else if (key == "remoteversion") {
            remoteversion = val;
        } else if (key == "remoteAPI") {
            remoteAPI = val;
        } else {
            std::cerr << "Unknown key type. " << key.toStdString() << " Something is very wrong!" << std::endl;
        }
    }
};

Q_DECLARE_METATYPE(Model)

class ModelManager : public QAbstractTableModel {
        Q_OBJECT
public:
    ModelManager(QObject *parent);
    void loadSettings();
    void writeModel(QString filename, QByteArray data);

    QList<Model> installedModels() const;
    QList<RemoteModel> remoteModels() const;
    QList<RemoteModel> availableModels() const; // remote - local

    enum Column {
        ModelName,
        ShortName,
        PathName,
        Type
    };
    Q_ENUM(Column);

    virtual QVariant data(QModelIndex const &, int role = Qt::DisplayRole) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    virtual int columnCount(QModelIndex const & = QModelIndex()) const;
    virtual int rowCount(QModelIndex const & = QModelIndex()) const;

    translateLocally::marianSettings& getSettings() {
        return settings_;
    }

public slots:
    void fetchRemoteModels();
    
private:
    void startupLoad();
    void scanForModels(QString path);
    void extractTarGz(QString filename);
    Model parseModelInfo(QJsonObject& obj, bool local=true);
    void parseRemoteModels(QJsonObject obj);
    QJsonObject getModelInfoJsonFromDir(QString dir);

    QSettings qset_;
    QDir configDir_;

    QStringList archives_; // Only archive name, not full path
    QList<Model> localModels_;
    QList<RemoteModel> remoteModels_;
    translateLocally::marianSettings settings_; // @TODO to be initialised by reading saved settings from disk

    QNetworkAccessManager *nam_;

    constexpr static int kColumnCount = 4;

signals:
    void newModelAdded(int index);
    void fetchedRemoteModels();
    void error(QString);
};

#endif // MODELMANAGER_H
