#ifndef MODELMANAGER_H
#define MODELMANAGER_H
#include <QDir>
#include <QList>
#include <QFuture>
#include <QAbstractTableModel>
#include <iostream>
#include "Network.h"
#include "types.h"


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

    // Is-equal operator for removing models from list
    inline bool operator==(const Model &other) const {
        return isSameModel(other)
            && (!isLocal() || path == other.path)
            && (!isRemote() || url == other.url);
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

    /**
     * @Brief extract a model into the directory of models managed by this
     * program. The optional filename argument is used to make up a folder name
     * for the model. If none provided, the basename of file is used. On success
     * the model is added to the local list of models (i.e. getInstalledModel())
     * and the function will return the filled in model instance. On failure, an
     * empty Model object is returned (i.e. model.isLocal() returns false).
     */
    Model writeModel(QFile *file, QString filename = QString());

    /**
     * @Brief Tries to delete a model from the getInstalledModels() list. Also
     * removes the files. Only managed models can be deleted this way.
     */
    bool removeModel(Model const &model);

    /**
     * @Brief is this model managed by ModelManager (i.e. created with 
     * writeModel()).
     */
    bool isManagedModel(Model const &model) const;

    /**
     * @Brief returns model from getInstalledModels() that matches the path.
     * Useful for checking whether a model for which you've saved the path
     * is still available.
     */
    Model getModelForPath(QString path) const; 

    /**
     * @Brief list of locally available models
     */
    QList<Model> getInstalledModels() const;

    /**
     * @Brief list of remotely available models. Only populated after
     * fetchRemoteModels is called and the fetchedRemoteModels() signal is
     * emitted.
     */
    QList<Model> getRemoteModels() const;

    /**
     * @Brief list of models that is both available locally and remote, but
     * the remote version is newer. Only available after fetchRemoteModels().
     */
    QList<Model> getUpdatedModels() const;

    /**
     * @Brief list of models that is available remote and not also installed
     * locally already. Only available after fetchRemoteModels().
     */

    QList<Model> getNewModels() const;
    
    /**
     * @brief whether or not fetchRemoteModels is in progress
     */
    inline bool isFetchingRemoteModels() const {
        return isFetchingRemoteModels_;
    }

    enum Column {
        Name,
        Version
    };

    Q_ENUM(Column);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

public slots:
    /**
     * @Brief downloads list of available remote models. On start, fetchingRemoteModels is emitted
     * (unless a fetch request is already in progress). When the request is
     * finished (either successfully or not) fetchedRemoteModels is emitted.
     * On success localModelsChanged() will also be emitted as fetching remote
     * models causes updates on the outdated() status of local models.
     */
    void fetchRemoteModels();
    
private:
    void startupLoad();
    void scanForModels(QString path);
    bool extractTarGz(QFile *file, QDir const &destination, QStringList &files);
    bool extractTarGzInCurrentPath(QFile *file, QStringList &files);
    Model parseModelInfo(QJsonObject& obj, translateLocally::models::Location type=translateLocally::models::Location::Local);
    void parseRemoteModels(QJsonObject obj);
    QJsonObject getModelInfoJsonFromDir(QString dir);
    
    /**
     * @Brief insert a local model in the localModels_ list. Keeps it sorted and
     * any views attached in sync. Will return True if the model is new, false
     * if it updates an existing entry.
     */
    bool insertLocalModel(Model model);

    /**
     * @Brief validate a model, currently by trying to parse the model_info.json
     * file with getModelInfoJsonFromDir(QString) and parseModelInfo(QJsonObject).
     * Will return true if it thinks the model can be loaded. If an error is
     * encountered, it will emit error(QString) signals with error messages.
     */
    bool validateModel(QString path);

    /**
     * @Brief updates getNewModels() and getUpdatedModels() lists. Emits the
     * localModelsChanged() signal. Possibly also the dataChanged() signal if
     * an installed model appears to be outdated.
     */
     void updateAvailableModels();

    QDir configDir_;

    QStringList archives_; // Only archive name, not full path
    QList<Model> localModels_;
    QList<Model> remoteModels_;
    QList<Model> newModels_;
    QList<Model> updatedModels_;

    Network *network_;
    bool isFetchingRemoteModels_;

signals:
    void fetchingRemoteModels();
    void fetchedRemoteModels(); // when finished fetching (might be error)
    void localModelsChanged();
    void error(QString);
};

#endif // MODELMANAGER_H
