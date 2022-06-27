#ifndef MODELMANAGER_H
#define MODELMANAGER_H
#include <QDir>
#include <QMap>
#include <QList>
#include <QJsonObject>
#include <QFuture>
#include <QAbstractTableModel>
#include <iostream>
#include <optional>
#include <type_traits>

#include "Network.h"
#include "types.h"
#include "settings/Settings.h"
#include "RepoManager.h"

namespace translateLocally {
    namespace models {
        enum Location {
            Remote = 0,
            Local = 1
        };
    }
}

/**
 * @Brief outside info about a model that it cannot know about itself in a
 * shipped json file like where did we get it from, when did we get it, etc.
 */
struct ModelMeta {
    QString path;          // local path to model directory
    QString modelUrl;         // url the model was downloaded from
    QString repositoryUrl;    // url of repository the model was mentioned by
    QDateTime installedOn; // utc datetime of the model's installation
};

Q_DECLARE_METATYPE(ModelMeta);

struct Model : ModelMeta {
    QString shortName; // Unique model identifier eg en-es-tiny
    QString modelName; // Long name, to be displayed in a single line
    QString url;
    QString src;
    QString trg;
    QMap<QString, QVariant> srcTags; // The second QVariant is a QString. This is done so that we can have direct toJson and fromJson conversion.
    QString trgTag;
    QString type; // Base or tiny
    QByteArray checksum;
    int localversion  = -1;
    int localAPI = -1;
    int remoteversion = -1;
    int remoteAPI = -1;

    template<class T>
    inline void set(QString key, T val) {
        bool parseError = false;
        if constexpr (std::is_same_v<QString, T>) {
            if (key == "shortName") {
                shortName = val;
            } else if (key == "modelName") {
                modelName = val;
            } else if (key == "url") {
                url = val;
            } else if (key == "src") {
                src = val;
            } else if (key == "trg") {
                trg = val;
            } else if (key == "trgTag") {
                trgTag = val;
            } else if (key == "type") {
                type = val;
            } else if (key == "checksum") {
                checksum = QByteArray::fromHex(val.toUtf8());
            } else {
                parseError = true; // TODO: this is just an unknown key in the json, that's not so bad is it?
            }
        } else if constexpr (std::is_same_v<int, T>) {
            if (key == "localversion") {
                localversion = val;
            } else if (key == "localAPI") {
                localAPI = val;
            } else if (key == "remoteversion") {
                remoteversion = val;
            } else if (key == "remoteAPI") {
                remoteAPI = val;
            } else {
                parseError = true; // TODO Idem.
            }
        } else if constexpr (std::is_same_v<QJsonObject, T>) {
            if (key == "srcTags") {
                srcTags = val.toVariantMap();
            } else {
               parseError = true; // TODO Again
            }
        }
        if (parseError) {
            qDebug() << "Model::set() was called with an unknown key type for key" << key;
        }
    }

    inline QString id() const {
        // @TODO make this something globally unique (so not just depended on what is in the JSON)
        // but also something that stays the same before/after downloading the model.
        return QString("%1%2").arg(shortName).arg(qHash(repositoryUrl));
    }

    inline bool isLocal() const {
        return !path.isEmpty();
    }

    inline bool isRemote() const {
        return !url.isEmpty();
    }

    inline bool isSameModel(Model const &model) const {
        return id() == model.id();
    }

    inline bool operator<(const Model& other) const {
        return shortName < other.shortName;
    }

    inline bool outdated() const {
        return localversion < remoteversion || localAPI < remoteAPI;
    }

    // Is-equal operator for removing models from list
    inline bool operator==(const Model &other) const {
        return isSameModel(other)
            && (!isLocal() || path == other.path)
            && (!isRemote() || url == other.url);
    }

    // Debug
    inline void print() const {
        qDebug() << "shortName:" << shortName
                 << "modelName:" << modelName
                 << "url:" << url
                 << "path:" << path
                 << "src:" << src
                 << "trg:" << trg
                 << "type:" << type
                 << "localversion" << localversion
                 << "localAPI" << localAPI
                 << "remoteversion:" << remoteversion
                 << "remoteAPI" << remoteAPI;
    }
    /**
     * @brief toJson Returns a json representation of the model. The only difference between the struct is that url and path will not be part of the json.
     *               Instead, we will have one bool that says "Is it local, or is it remote". We also don't report checksums and API versions as those
     *               should be handled by the backend. Used by NativeMessaging interface to describe available models.
     * @return Json representation of a model
     */
     QJsonObject toJson() const {
        QJsonObject ret;
        ret["id"] = id();
        ret["shortname"] = shortName;
        ret["modelName"] = modelName;
        ret["local"] = isLocal();
        ret["src"] = src;
        ret["trg"] = trg;
        ret["srcTags"] = QJsonObject::fromVariantMap(srcTags);
        ret["trgTag"] = trgTag;
        ret["type"] = type;
        ret["repositoryUrl"] = repositoryUrl;
        return ret;
    }
};

Q_DECLARE_METATYPE(Model)

/**
 * @Brief model pair for src -> pivot -> trg translation.
 */
struct ModelPair {
    Model model;
    Model pivot;
};

Q_DECLARE_METATYPE(ModelPair)

class ModelManager : public QAbstractTableModel {
        Q_OBJECT
public:
    ModelManager(QObject *parent, Settings *settings);

    /**
     * @Brief get model by its id
     */
    std::optional<Model> getModel(QString const &id) const;

    /**
     * @Brief find model to translate directly from src to trg language.
     */
    std::optional<Model> getModelForLanguagePair(QString src, QString trg) const;

    /**
     * @Brief find model to translate via pivot from src to trg language.
     */
    std::optional<ModelPair> getModelPairForLanguagePair(QString src, QString trg, QString pivot = QString("en")) const;

    /**
     * @Brief extract a model into the directory of models managed by this
     * program. The optional filename argument is used to make up a folder name
     * for the model. If none provided, the basename of file is used. On success
     * the model is added to the local list of models (i.e. getInstalledModel())
     * and the function will return the filled in model instance. On failure, an
     * empty Model object is returned (i.e. model.isLocal() returns false).
     */
    std::optional<Model> writeModel(QFile *file, ModelMeta meta = ModelMeta(), QString filename = QString());

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
    std::optional<Model> getModelForPath(QString path) const;

    /**
     * @Brief list of locally available models
     */
    const QList<Model>& getInstalledModels() const;

    /**
     * @Brief list of remotely available models. Only populated after
     * fetchRemoteModels is called and the fetchedRemoteModels() signal is
     * emitted.
     */
    const QList<Model>& getRemoteModels() const;

    /**
     * @Brief list of models that is both available locally and remote, but
     * the remote version is newer. Only available after fetchRemoteModels().
     */
    const QList<Model>& getUpdatedModels() const;

    /**
     * @Brief list of models that is available remote and not also installed
     * locally already. Only available after fetchRemoteModels().
     */

    const QList<Model>& getNewModels() const;

    RepoManager * getRepoManager();
    
    /**
     * @brief whether or not fetchRemoteModels is in progress
     */
    inline bool isFetchingRemoteModels() const {
        return isFetchingRemoteModels_;
    }

    enum Column {
        Name,
        Repository,
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
     * By default, it fetches models from the official translateLocally repo, but can also fetch
     * models from a 3rd party repository.
     *
     * @param extradata Optional argument that is indended if we want to pass extra data to the slot
     */
    void fetchRemoteModels(QVariant extradata = QVariant());
    
private:
    void startupLoad();
    void scanForModels(QString path);
    bool extractTarGz(QFile *file, QDir const &destination, QStringList &files);
    bool extractTarGzInCurrentPath(QFile *file, QStringList &files);
    std::optional<Model> parseModelInfo(QJsonObject& obj, translateLocally::models::Location type=translateLocally::models::Location::Local, QString *error = nullptr);
    void parseRemoteModels(QJsonObject obj, QString repositoryUrl);
    QJsonObject getModelInfoJsonFromDir(QString dir, QString *error = nullptr);

    /**
     * @Brief gets model metadata from an installed model
     */
    bool readModelMetaFromDir(ModelMeta &model, QString dir) const;

    /**
     * @Brief writes a model's metadata to an installed model.
     */
    bool writeModelMetaToDir(ModelMeta const &model, QString dir) const;
    
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
    Settings *settings_;
    RepoManager repositories_;
    bool isFetchingRemoteModels_;

signals:
    void fetchingRemoteModels();
    void fetchedRemoteModels(QVariant extradata =  QVariant()); // when finished fetching (might be error)
    void localModelsChanged();
    void error(QString);
};

#endif // MODELMANAGER_H
