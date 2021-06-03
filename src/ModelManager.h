#ifndef MODELMANAGER_H
#define MODELMANAGER_H
#include <QDir>
#include <QList>
#include <QFuture>
#include <iostream>
#include "types.h"

class QNetworkAccessManager;
class QByteArray;

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

    inline bool isSameModel(Model const &model) const {
        // TODO: matching by name might not be very robust
        return shortName == model.shortName;
    }

    inline bool operator<(const Model& other) const {
        return shortName < other.shortName;
    }

    inline bool outdated() const {
        return localversion<remoteversion || localAPI < remoteAPI;
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

class ModelManager : public QObject {
        Q_OBJECT
public:
    ModelManager(QObject *parent);
    void loadSettings();
    Model writeModel(QString filename, QByteArray data);

    QList<Model> getInstalledModels() const;
    QList<Model> getRemoteModels() const;
    QList<Model> getUpdatedModels() const;
    QList<Model> getNewModels() const;
    /**
     * @brief updateAvailableModels once new models are fetched from the interwebs, you update the local models with version information
     *                        and make a list of new models as well as models that are just updates of local models, so that the
     *                        user can download a new version.
     */
    void updateAvailableModels(); // remote - local

    translateLocally::marianSettings& getSettings() {
        return settings_;
    }

public slots:
    void fetchRemoteModels();
    
private:
    void startupLoad();
    void scanForModels(QString path);
    bool extractTarGz(QByteArray data);
    bool extractTarGzInCurrentPath(QByteArray data);
    Model parseModelInfo(QJsonObject& obj, translateLocally::models::Location type=translateLocally::models::Location::Local);
    void parseRemoteModels(QJsonObject obj);
    QJsonObject getModelInfoJsonFromDir(QString dir);
    bool insertLocalModel(Model model);

    QDir configDir_;

    QStringList archives_; // Only archive name, not full path
    QList<Model> localModels_;
    QList<Model> remoteModels_;
    QList<Model> newModels_;
    QList<Model> updatedModels_;
    translateLocally::marianSettings settings_; // @TODO to be initialised by reading saved settings from disk

    QNetworkAccessManager *nam_;

    constexpr static int kColumnCount = 4;

signals:
    void newModelAdded(int index);
    void fetchedRemoteModels();
    void localModelsChanged();
    void error(QString);
};

#endif // MODELMANAGER_H
