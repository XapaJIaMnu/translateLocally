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

namespace translateLocally {
    namespace models {
        enum Location {
            Remote = 0,
            Local = 1
        };
    }
}

struct Model {
    QString shortName; // Unique model identifier eg en-es-tiny
    QString modelName; // Long name, to be displayed in a single line
    QString url;  // This is the url to the model. Only available if we connected to the server
    QString path; // This is full path to the directory. Only available if the model is local
    QString src;
    QString trg;
    QString type; // Base or tiny
    float localversion  = -1.0f;
    float localAPI = -1.0f;
    float remoteversion = -1.0f;
    float remoteAPI = -1.0f;

    inline void set(QString key, QString val) {
        if (key == "shortName") {
            shortName = val;
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
    inline void set(QString key, float val) {
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

    inline bool isLocal() const {
        return !path.isEmpty();
    }

    inline bool isRemote() const {
        return !url.isEmpty();
    }

    inline bool operator<(const Model& other) const {
        return shortName < other.shortName;
    }

    // Debug
    inline void print() const {
        std::cerr << "shortName: " << shortName.toStdString() << " modelName: " << modelName.toStdString() <<
                     " url: " << url.toStdString() << " path " << path.toStdString() << " src " << src.toStdString() << " trg " << trg.toStdString() <<
                     " type: " << type.toStdString() << " localversion " << localversion << " localAPI " << localAPI <<
                     " remoteversion: " << remoteversion << " remoteAPI " << remoteAPI << std::endl;
    }
};

Q_DECLARE_METATYPE(Model)

class ModelManager : public QAbstractTableModel {
        Q_OBJECT
public:
    ModelManager(QObject *parent);
    void loadSettings();
    Model writeModel(QString filename, QByteArray data);

    QList<Model> installedModels() const;
    QList<Model> remoteModels() const;
    QList<Model> availableModels() const; // remote - local

    enum Column {
        ModelName,
        ShortName,
        PathName,
        Type
    };
    Q_ENUM(Column);

    // @TODO those can removed now that we don't have two model types
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
    Model parseModelInfo(QJsonObject& obj, translateLocally::models::Location type=translateLocally::models::Location::Local);
    void parseRemoteModels(QJsonObject obj);
    QJsonObject getModelInfoJsonFromDir(QString dir);

    QSettings qset_;
    QDir configDir_;

    QStringList archives_; // Only archive name, not full path
    QList<Model> localModels_;
    QList<Model> remoteModels_;
    translateLocally::marianSettings settings_; // @TODO to be initialised by reading saved settings from disk

    QNetworkAccessManager *nam_;

    constexpr static int kColumnCount = 4;

signals:
    void newModelAdded(int index);
    void fetchedRemoteModels();
    void error(QString);
};

#endif // MODELMANAGER_H
