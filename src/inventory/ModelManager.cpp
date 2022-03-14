#include "ModelManager.h"
#include "Network.h"
#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QTemporaryDir>
#include <iostream>
// libarchive
#include <archive.h>
#include <archive_entry.h>
#include <algorithm>
#include <optional>
#include <variant>

namespace {
    /**
     * Give it a QStringList with multiple paths, and this will return the
     * path prefix (i.e. the path to the shared root directory). Note: if only
     * a single path is given, the prefix is the dirname part of the path.
     */
    QString getCommonPrefixPath(QStringList list) {
        auto it = list.begin();

        if (it == list.end())
            return QString();

        QString prefix = *it++;

        for (; it != list.end(); ++it) {
            auto offsets = std::mismatch(
                prefix.begin(), prefix.end(),
                it->begin(), it->end());

            // If we found a mismatch, we found a substring in it that's a
            // shorter common prefix.
            if (offsets.first != prefix.end())
                prefix = it->left(offsets.second - it->begin());
        }

        // prefix.section("/", 0, -1)?
        return prefix.section("/", 0, -2);
    }

    std::optional<Model> findModel(QList<Model> const &models, QString src, QString trg) {
        std::optional<Model> found;

        for (auto &&model : models) {
            // Skip models that do not match src & trg
            // @TODO deal with 'en' vs 'en-US'
            if (!model.srcTags.contains(src) || model.trgTag != trg)
                continue;

            if (!found || (found->type != "tiny" && model.type == "tiny"))
                found = model;
        }

        return found;
    }
}


ModelManager::ModelManager(QObject *parent, Settings * settings)
    : QAbstractTableModel(parent)
    , network_(new Network(this))
    , isFetchingRemoteModels_(false)
    , repositories_(this, settings)
{
    // Create/Load Settings and create a directory on the first run. Use mock QSEttings, because we want nativeFormat, but we don't want ini on linux.
    // NativeFormat is not always stored in config dir, whereas ini is always stored. We used the ini format to just get a path to a dir.
    configDir_ = QFileInfo(QSettings(QSettings::IniFormat, QSettings::UserScope, "translateLocally", "translateLocally").fileName()).absoluteDir();
    if (!QDir(configDir_).exists()) {
        if (QFileInfo::exists(configDir_.absolutePath())) {
            std::cerr << "We want to store data at a directory at: " << configDir_.absolutePath().toStdString() << " but a file with the same name exists." << std::endl;
        } else {
            QDir().mkpath(configDir_.absolutePath());
        }
    }
    startupLoad();
    // Fetch remote models after new a new entry was added. Lambda wrapped to use the new syntax without explicit slot.
    connect(&repositories_, &RepoManager::rowsInserted, this, [&](){fetchRemoteModels();});
}

bool ModelManager::isManagedModel(Model const &model) const {
    return model.isLocal() && model.path.startsWith(configDir_.absolutePath());
}

bool ModelManager::validateModel(QString path) {
    QJsonObject obj = getModelInfoJsonFromDir(path);
    if (obj.find("path") == obj.end()) {
        emit error(tr("Failed to find, open or parse the model_info.json in %1").arg(path));
        return false;
    }

    if (!parseModelInfo(obj).isLocal()) // parseModelInfo emits its own error signals
        return false;

    return true;
}

std::optional<Model> ModelManager::getModel(QString const &id) const {
    for (auto &&model : getInstalledModels())
        if (model.id() == id)
            return model;

    for (auto &&model : getRemoteModels())
        if (model.id() == id)
            return model;

    return std::nullopt;
}

std::optional<Model> ModelManager::getModelForLanguagePair(QString src, QString trg) const {
    // First search the already installed models.
    std::optional<Model> found(findModel(getInstalledModels(), src, trg));
    
    // Did we find an installed model? If not, search the remote models
    if (!found)
        found = findModel(getRemoteModels(), src, trg);

    return found;
}

std::optional<ModelPair> ModelManager::getModelPairForLanguagePair(QString src, QString trg, QString pivot) const {
    std::optional<Model> sourceModel = getModelForLanguagePair(src, pivot);
    if (!sourceModel)
        return std::nullopt;

    std::optional<Model> pivotModel = getModelForLanguagePair(pivot, trg);
    if (!pivotModel)
        return std::nullopt;

    return ModelPair{*sourceModel, *pivotModel};
}

std::optional<Model> ModelManager::writeModel(QFile *file, QString filename) {
    // Default value for filename is the basename of the file.
    if (filename.isEmpty())
        filename = QFileInfo(*file).fileName();

    // Initially extract to to a temporary directory. Will delete its contents
    // when it goes out of scope. Creating a temporary directory specifically
    // inside the target directory to make sure we're on the same filesystem.
    // Otherwise `QDir::rename()` might fail. Note that directories starting
    // with "extracting-" are explicitly skipped `scanForModels()`.
    QTemporaryDir tempDir(configDir_.filePath("extracting-XXXXXXX"));
    if (!tempDir.isValid()) {
        emit error(tr("Could not create temporary directory in %1 to extract the model archive to.").arg(configDir_.path()));
        return std::nullopt;
    }

    // Try to extract the archive to the temporary directory
    QStringList extracted;
    if (!extractTarGz(file, tempDir.path(), extracted))
        return std::nullopt;

    // Assert we extracted at least something.
    if (extracted.isEmpty()) {
        emit error(tr("Did not extract any files from the model archive."));
        return std::nullopt;
    }

    // Get the common prefix of all files. In the ideal case, it's the same as
    // tempDir, but the archive might have had it's own sub folder.
    QString prefix = getCommonPrefixPath(extracted);
    if (prefix.isEmpty()) {
        emit error(tr("Could not determine prefix path of extracted model."));
        return std::nullopt;
    }

    Q_ASSERT(prefix.startsWith(tempDir.path()));

    // Try determining whether the model is any good before we continue to safe
    // it to a permanent destination
    if (!validateModel(prefix)) // validateModel emits its own error() signals (hence validateModel and not isModelValid)
        return std::nullopt;

    QString newModelDirName = QString("%1-%2").arg(filename.split(".tar.gz")[0]).arg(QDateTime::currentMSecsSinceEpoch() / 1000);
    QString newModelDirPath = configDir_.absoluteFilePath(newModelDirName);

    if (!QDir().rename(prefix, newModelDirPath)) {
        emit error(tr("Could not move extracted model from %1 to %2.").arg(tempDir.path(), newModelDirPath));
        return std::nullopt;
    }

    // Only remove the temp directory if we moved a directory within it. Don't 
    // attempt anything if we moved the whole directory itself.
    tempDir.setAutoRemove(prefix != tempDir.path());

    QJsonObject obj = getModelInfoJsonFromDir(newModelDirPath);
    Q_ASSERT(obj.find("path") != obj.end());

    Model model = parseModelInfo(obj);

    // Upgrade behaviour: remove any older versions of this model. At least if
    // the older model is part of the models managed by us. We don't delete
    // models from the CWD.
    // Note: Right now there's no check on version. We assume that if writeModel
    // is called, it either was called from the upgrade path, or the user
    // intentionally installing an older model through the model manager UI.
    for (auto &&installed : localModels_)
        if (installed.isSameModel(model) && isManagedModel(installed))
            removeModel(installed);

    insertLocalModel(model);
    updateAvailableModels();
    
    return model;
}

bool ModelManager::removeModel(Model const &model) {
    if (!isManagedModel(model))
        return false;

    QDir modelDir = QDir(model.path);

    // First attempt to remove the model_info.json file as a test. If that works
    // we know that at least the model won't be loaded on next scan/start-up.

    if (!modelDir.remove("model_info.json")) {
        emit error(tr("Could not delete %1/model_info.json").arg(model.path));
        return false;
    }

    if (!modelDir.removeRecursively()) {
        emit error(tr("Could not completely remove the model directory %1").arg(model.path));
        // no return here because we did remove model_info.json already, so we
        // should also remove the model from localModels_
    }

    int position = localModels_.indexOf(model);

    if (position == -1)
        return false;

    beginRemoveRows(QModelIndex(), position, position);
    localModels_.removeOne(model);
    endRemoveRows();
    updateAvailableModels();
    return true;
}

bool ModelManager::insertLocalModel(Model model) {
    int position = 0;

    for (int i = 0; i < localModels_.size(); ++i) {
        // First, make sure we don't already have this model
        if (localModels_[i].isSameModel(model)) {
            localModels_[i] = model;
            emit dataChanged(index(i, 0), index(i, columnCount()));
            return false;
        }

        // Second, while we're iterating anyway, figure out where to insert
        // this model.
        if (localModels_[i] < model)
            position = i + 1;
    }

    beginInsertRows(QModelIndex(), position, position);
    localModels_.insert(position, model);
    endInsertRows();
    return true;
}

QJsonObject ModelManager::getModelInfoJsonFromDir(QString dir) {
    // Check if we can find a model_info.json in the directory. If so, record it as part of the model
    QFileInfo modelInfo(dir + "/model_info.json");
    if (modelInfo.exists()) {
        QFile modelInfoFile(modelInfo.absoluteFilePath());
        bool isOpen = modelInfoFile.open(QIODevice::ReadOnly | QIODevice::Text);
        if (isOpen) {
            QByteArray bytes = modelInfoFile.readAll();
            modelInfoFile.close();
            // Parse the Json
            QJsonDocument jsonResponse = QJsonDocument::fromJson(bytes);
            QJsonObject obj = jsonResponse.object();
            // Populate the json with path
            obj.insert(QString("path"), QJsonValue(dir));
            return obj;
        } else {
            emit error(tr("Failed to open json config file: %1").arg(modelInfo.absoluteFilePath()));
            return QJsonObject();
        }
    } else {
        // Model info doesn't exist or a configuration file is not found. Handle the error elsewhere.
        return QJsonObject();
    }
}

Model ModelManager::parseModelInfo(QJsonObject& obj, translateLocally::models::Location type) {
    using namespace translateLocally::models;
    std::vector<QString> keysSTR = {QString{"shortName"},
                                    QString{"modelName"},
                                    QString{"src"},
                                    QString{"trg"},
                                    QString("trgTag"),
                                    QString{"type"},
                                    QString("repository"),
                                    QString{"checksum"}};
    std::vector<QString> keysINT{QString("version"), QString("API")};
    QString criticalKey = type==Local ? QString("path") : QString("url");

    Model model = {};
    // Non critical keys. Some of them might be missing from old model versions but we don't care
    for (auto&& key : keysSTR) {
        auto iter = obj.find(key);
        if (iter != obj.end()) {
            model.set(key, iter.value().toString());
        }
    }

    // Int Keys depend on whether we have a local or a remote model
    // Non critical if missing due to older model version.
    for (auto&& key : keysINT) {
        QString keyname = type==Local ? "local" + key : "remote" + key;
        auto iter = obj.find(key);
        if (iter != obj.end()) {
            model.set(keyname, iter.value().toInt());
        }
    }

    // srcTags keys. It's a json object. Non-critical.
    {
        auto iter = obj.find(QString("srcTags"));
        if (iter != obj.end()) {
            model.set("srcTags", iter.value().toObject());
        }
    }

    // Fill in srcTags based on model name if it is an old-style model_info.json
    {
        // split 'eng-ukr-tiny11' into 'eng', 'ukr', and the rest.
        auto parts = model.shortName.split('-');
        if (parts.size() > 2) {
            if (model.srcTags.isEmpty())
                model.srcTags = {{parts[0], model.src}};
            if (model.trgTag.isEmpty())
                model.trgTag = parts[1];
        }
    }

    // Critical key. If this key is missing the json is completely invalid and needs to be discarded
    // it's either the path to the model or the url to its download location
    auto iter = obj.find(criticalKey);
    if (iter != obj.end()) {
        model.set(criticalKey, iter.value().toString());
    } else {
        emit error(tr("The json file provided is missing '%1' or is corrupted. Please redownload the model. "
                      "If the path variable is missing, it is added automatically, so please file a bug report at: https://github.com/XapaJIaMnu/translateLocally/issues").arg(criticalKey));
        return Model{};
    }
    return model;
}

void ModelManager::scanForModels(QString path) {
    //Iterate over all files in the folder and take note of available models and archives
    //@TODO currently, archives can only be extracted from the config dir
    QDirIterator it(path, QDir::NoFilter);
    while (it.hasNext()) {
        QString current = it.next();
        QFileInfo f(current);
        if (f.isDir()) {
            // Skip temporary directories created by `writeModel()`.
            if (f.baseName().startsWith("extracting-"))
                continue;

            QJsonObject obj = getModelInfoJsonFromDir(current);
            if (!obj.empty()) {
                Model model = parseModelInfo(obj);
                if (model.path != "") {
                    insertLocalModel(model);
                } else {
                    emit error(tr("Corrupted json file: %1/model_info.json. Delete or redownload.").arg(current));
                }
            } else {
                // We have a folder in our models directory that doesn't contain a model. This is ok.
                continue;
            }
        } else {
            // Check if this an existing archive
            if (f.completeSuffix() == QString("tar.gz")) {
                archives_.append(f.fileName());
            }
        }
    }

    updateAvailableModels();
}

void ModelManager::startupLoad() {
    //Iterate over all files in the config folder and take note of available models and archives
    scanForModels(configDir_.absolutePath());
    scanForModels(QDir::current().path()); // Scan the current directory for models. @TODO archives found in this folder would not be used
}

// Adapted from https://github.com/libarchive/libarchive/blob/master/examples/untar.c#L136
bool ModelManager::extractTarGz(QFile *file, QDir const &destination, QStringList &files) {
    // Change current working directory while extracting
    QString currentPath = QDir::currentPath();

    if (!QDir::setCurrent(destination.absolutePath())) {
        emit error(tr("Failed to change path to the configuration directory %1. %2 won't be extracted.").arg(destination.absolutePath(), file->fileName()));
        return false;
    }

    QStringList extracted;
    bool success = extractTarGzInCurrentPath(file, extracted);

    for (QString const &file : qAsConst(extracted))
        files << destination.filePath(file);

    QDir::setCurrent(currentPath);
    return success;
}

bool ModelManager::extractTarGzInCurrentPath(QFile *file, QStringList &files) {
    auto warn = [&](const char *f, const char *m) {
        emit error(tr("Trouble while extracting language model after call to %1: %2").arg(f, m));
    };

    auto copy_data = [=](struct archive *a_in, struct archive *a_out) {
        const void *buff;
        size_t size;
#if ARCHIVE_VERSION_NUMBER >= 3000000
        int64_t offset;
#else
        off_t offset;
#endif

        for (;;) {
            int retval = archive_read_data_block(a_in, &buff, &size, &offset);
            // End of archive: good!
            if (retval == ARCHIVE_EOF)
                return ARCHIVE_OK;
            
            // Not end of archive: bad.
            if (retval != ARCHIVE_OK) {
                warn("archive_read_data_block()", archive_error_string(a_in));
                return retval;
            }
            
            retval = archive_write_data_block(a_out, buff, size, offset);
            if (retval != ARCHIVE_OK) {
                warn("archive_write_data_block()", archive_error_string(a_out));
                return retval;
            }
        }
    };

    archive *a_in = archive_read_new();
    archive *a_out = archive_write_disk_new();
    archive_write_disk_set_options(a_out, ARCHIVE_EXTRACT_TIME);
    
    archive_read_support_format_tar(a_in);
    archive_read_support_filter_gzip(a_in);
    
    if (!file->open(QIODevice::ReadOnly)) {
        emit error(tr("Trouble while extracting language model after call to %1: %2").arg("QIODevice::open()", file->errorString()));
        return false;
    }
    
    if (archive_read_open_fd(a_in, file->handle(), 10240)) {
        warn("archive_read_open_filename()", archive_error_string(a_in));
        return false;
    }

    // Read (and extract) all archive entries
    for (;;) {
        archive_entry *entry;
        
        int retval = archive_read_next_header(a_in, &entry);

        // Stop when we read past the last entry
        if (retval == ARCHIVE_EOF)
            break;

        if (retval < ARCHIVE_OK)
            warn("archive_read_next_header()", archive_error_string(a_in));
        if (retval < ARCHIVE_WARN)
            return false;

        retval = archive_write_header(a_out, entry);
        if (retval < ARCHIVE_OK)
            warn("archive_write_header()", archive_error_string(a_out));
        else {
            files << QString(archive_entry_pathname(entry));

            if(archive_entry_size(entry) > 0)
                if (copy_data(a_in, a_out) < ARCHIVE_WARN)
                    return false;
        }

        retval = archive_write_finish_entry(a_out);
        if (retval < ARCHIVE_OK)
            warn("archive_write_finish_entry()", archive_error_string(a_out));
        if (retval < ARCHIVE_WARN)
            return false;
    }

    archive_read_close(a_in);
    archive_read_free(a_in);

    archive_write_close(a_out);
    archive_write_free(a_out);

    return true;
}

void ModelManager::fetchRemoteModels(QVariant extradata) {
    if (isFetchingRemoteModels())
        return;

    QStringList repos = repositories_.getRepos();
    QSharedPointer<int> num_repos(new int(repos.size())); // Keep track of how many repos have been fetched
    for (auto&& urlString : repos) {
        isFetchingRemoteModels_ = true;
        emit fetchingRemoteModels();

        QUrl url(urlString);
        QNetworkRequest request(url);
        QNetworkReply *reply = network_->get(request);
        connect(reply, &QNetworkReply::finished, this, [=] {
            switch (reply->error()) {
                case QNetworkReply::NoError:
                    parseRemoteModels(QJsonDocument::fromJson(reply->readAll()).object());
                    break;
                default:
                    QString errstr = QString("Error fetching remote repository: ") + urlString +
                            QString("\nError code: ") + reply->errorString() +
                            QString("\nPlease double check that the address is reachable.");
                    emit error(errstr);
                    break;
            }
            if (--(*num_repos) == 0) { // Once we have fetched all repositories, re-enable fetch.
                isFetchingRemoteModels_ = false;
                emit fetchedRemoteModels(extradata);
            }

            reply->deleteLater();
        });
    }
}

void ModelManager::parseRemoteModels(QJsonObject obj) {
    using namespace translateLocally::models;
    
    bool empty = true;
    for (auto&& arrobj : obj["models"].toArray()) {
        empty = false;
        QJsonObject obj = arrobj.toObject();
        Model remoteModel = parseModelInfo(obj, Remote);
        if (!remoteModels_.contains(remoteModel)) { // This costs O(n). Not happy, is there a better way?
            remoteModels_.append(std::move(remoteModel));
        }
    }
    if (empty) {
        emit error("No models found in the repository. Please double check that the repository address is correct.");
    }

    std::sort(remoteModels_.begin(), remoteModels_.end());
    updateAvailableModels();
}

const QList<Model>& ModelManager::getInstalledModels() const {
    return localModels_;
}

const QList<Model>& ModelManager::getRemoteModels() const {
    return remoteModels_;
}

const QList<Model>& ModelManager::getNewModels() const {
    return newModels_;
}

const QList<Model>& ModelManager::getUpdatedModels() const {
    return updatedModels_;
}

std::optional<Model> ModelManager::getModelForPath(QString path) const {
    for (Model const &model : getInstalledModels())
        if (model.path == path)
            return model;

    return std::nullopt;
}

void ModelManager::updateAvailableModels() {
    newModels_.clear();
    updatedModels_.clear();

    for (auto &&model : remoteModels_) {
        bool installed = false;
        bool outdated = false;
        for (int i = 0; i < localModels_.size(); ++i) {
            if (localModels_[i].isSameModel(model)) {
                localModels_[i].remoteAPI = model.remoteAPI;
                localModels_[i].remoteversion = model.remoteversion;
                installed = true;
                outdated = localModels_[i].outdated();
                emit dataChanged(index(i, 0), index(i, columnCount()));
                break;
            }
        }

        if (!installed) {
            newModels_.append(model);
        }
        if (outdated) {
            updatedModels_.append(model);
        }
    }

    emit localModelsChanged();
}

RepoManager * ModelManager::getRepoManager() {
    return &repositories_;
}

int ModelManager::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);

    return localModels_.size();
} 

int ModelManager::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);

    return 3;
}

QVariant ModelManager::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation != Qt::Horizontal)
        return QVariant();

    switch (section) {
        case Column::Name:
            return tr("Name", "translation model name");
        case Column::Repository:
            return tr("Repository", "repository from which the translation model originated");
        case Column::Version:
            return tr("Version", "translation model version");
        default:
            return QVariant();
    }
}

QVariant ModelManager::data(const QModelIndex &index, int role) const {
    if (index.row() >= localModels_.size())
        return QVariant();

    Model model = localModels_[index.row()];

    if (role == Qt::UserRole)
        return QVariant::fromValue(model);

    switch (index.column()) {
        case Column::Name:
            switch (role) {
                case Qt::DisplayRole:
                    return model.modelName;
                default:
                    return QVariant();
            }

        case Column::Repository:
            switch (role) {
                case Qt::DisplayRole:
                    return model.repository;
                default:
                    return QVariant();
            }

        case Column::Version:
            switch (role) {
                case Qt::DisplayRole:
                    return model.localversion;
                case Qt::TextAlignmentRole:
                    // @TODO figure out how to compile combined flag as below. 
                    // Error is "can't convert the result to QVariant."
                    // return Qt::AlignRight | Qt::AlignBaseline;
                    return Qt::AlignCenter;
                default:
                    return QVariant();
            }
    }

    return QVariant();
}
