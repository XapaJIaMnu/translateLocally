#include "ModelManager.h"
#include "Network.h"
#include "types.h"
#include <QApplication>
#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QTemporaryDir>
#include <QtGui>
#include <QColor>
#include <QStyle>
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
    , settings_(settings)
    , isFetchingRemoteModels_(false)
{
    appDataDir_.setPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (!QDir(appDataDir_).exists()) {
        if (QFileInfo::exists(appDataDir_.absolutePath())) {
            std::cerr << "We want to store data at a directory at: " << appDataDir_.absolutePath().toStdString() << " but a file with the same name exists." << std::endl;
        } else {
            QDir().mkpath(appDataDir_.absolutePath());
        }
    }

    // Attempt to migrate model data previously located in the configuration
    // directory to the new location. If the move operation fails, the files
    // are left where they currently reside and are still discovered later.
    QDir configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDirIterator configDirIter = QDirIterator(configDir);
    while (configDirIter.hasNext()) {
        configDirIter.next();
        QFileInfo fileInfo = configDirIter.fileInfo();
        if (fileInfo.isDir() || fileInfo.fileName().endsWith(".tar.gz")) {
            QFile::rename(fileInfo.absoluteFilePath(), appDataDir_.filePath(fileInfo.fileName()));
        }
    }

    connect(&(settings_->repos), &Setting::valueChanged, this, [&]{
        // I disabled the call to fetch the remote models because I'm not
        // certain that the internet access is expected (and permitted) by the
        // end user at this point.
       // fetchRemoteModels();

        // We do clear the remoteModels list so that it is clear that it is
        // outdated and/or incomplete. Now users can click the "download model
        // list" again and make an informed decision to access the internet.
        remoteModels_.clear();
        updateAvailableModels();
    });

    startupLoad();
}

std::optional<Model> ModelManager::findModelForUpdate(Model const& model) {
    for (auto&& newmodel : getUpdatedModels()) {
        if (newmodel.id() == model.id()) {
            return newmodel;
         }
    }
    return std::nullopt;
}

bool ModelManager::isManagedModel(Model const &model) const {
    return model.isLocal() && model.path.startsWith(appDataDir_.absolutePath());
}

bool ModelManager::validateModel(QString path) {
    QString errorMsg;

    auto obj = getModelInfoJsonFromDir(path, &errorMsg);
    if (obj.isEmpty()) {
        emit error(errorMsg);
        return false;
    }

    auto model = parseModelInfo(obj, translateLocally::models::Location::Local, &errorMsg);
    if (!model) {
        emit error(tr("The model_info.json in %1 contains errors: %2").arg(path, errorMsg));
        return false;
    }

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

std::optional<Model> ModelManager::writeModel(QFile *file, ModelMeta meta, QString filename) {
    // Default value for filename is the basename of the file.
    if (filename.isEmpty())
        filename = QFileInfo(*file).fileName();

    // Initially extract to to a temporary directory. Will delete its contents
    // when it goes out of scope. Creating a temporary directory specifically
    // inside the target directory to make sure we're on the same filesystem.
    // Otherwise `QDir::rename()` might fail. Note that directories starting
    // with "extracting-" are explicitly skipped `scanForModels()`.
    QTemporaryDir tempDir(appDataDir_.filePath("extracting-XXXXXXX"));
    if (!tempDir.isValid()) {
        emit error(tr("Could not create temporary directory in %1 to extract the model archive to.").arg(appDataDir_.path()));
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

    // Assume the prefix is at least tempDir. If not, something shady is
    // happening, like the tar.gz file writing to an absolute path?
    Q_ASSERT(prefix.startsWith(tempDir.path()));

    // Try determining whether the model is any good before we continue to safe
    // it to a permanent destination
    if (!validateModel(prefix)) // validateModel emits its own error() signals (hence validateModel and not isModelValid)
        return std::nullopt;

    QString newModelDirName = QString("%1-%2").arg(filename.split(".tar.gz")[0]).arg(QDateTime::currentMSecsSinceEpoch() / 1000);
    QString newModelDirPath = appDataDir_.absoluteFilePath(newModelDirName);

    if (!QDir().rename(prefix, newModelDirPath)) {
        emit error(tr("Could not move extracted model from %1 to %2.").arg(tempDir.path(), newModelDirPath));
        return std::nullopt;
    }

    // Only remove the temp directory if we moved a directory within it. Don't 
    // attempt anything if we moved the whole directory itself.
    tempDir.setAutoRemove(prefix != tempDir.path());

    auto obj = getModelInfoJsonFromDir(newModelDirPath);
    if (obj.isEmpty()) return std::nullopt; // validateModel() has already emitted an error in this case
    
    auto model = parseModelInfo(obj, translateLocally::models::Local);
    if (!model) return std::nullopt; // same, validateModel will have raised an issue.

    model->path = newModelDirPath;

    writeModelMetaToDir(meta, model->path);
    // We need to update the current model with the metadata that we have.
    // I could have written 4 lines that do something like model.x = meta.x
    // But if we change the metalfile format, I'd have to change that here as well.
    // Instead I will abuse the readModelMetaFromDir function to avoid code duplication. Sue me.
    readModelMetaFromDir(*model, model->path);

    // Upgrade behaviour: remove any older versions of this model. At least if
    // the older model is part of the models managed by us. We don't delete
    // models from the CWD.
    // Note: Right now there's no check on version. We assume that if writeModel
    // is called, it either was called from the upgrade path, or the user
    // intentionally installing an older model through the model manager UI.
    for (auto &&installed : localModels_)
        if (installed.isSameModel(*model) && isManagedModel(installed))
            removeModel(installed);

    insertLocalModel(*model);
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

QJsonObject ModelManager::getModelInfoJsonFromDir(QString dir, QString *error) {
    // Check if we can find a model_info.json in the directory. If so, record it as part of the model
    QFileInfo modelInfo(dir + "/model_info.json");
    if (!modelInfo.exists()) {
        if (error) *error = tr("File %1 does not exist").arg(modelInfo.filePath());
        return QJsonObject();
    }

    QFile modelInfoFile(modelInfo.absoluteFilePath());
    if (!modelInfoFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = tr("File %1 cannot be opened for reading").arg(modelInfo.filePath());
        return QJsonObject();
    }
    
    QByteArray bytes = modelInfoFile.readAll();
    modelInfoFile.close();
    
    // Parse the Json
    QJsonParseError parseError;
    QJsonDocument jsonResponse = QJsonDocument::fromJson(bytes, &parseError);
    if (jsonResponse.isNull()) {
        if (error) *error = tr("%1 in file %2").arg(parseError.errorString(), modelInfo.filePath());
        return QJsonObject();
    }

    return jsonResponse.object();
}

std::optional<Model> ModelManager::parseModelInfo(QJsonObject& obj, translateLocally::models::Location type, QString *error) {
    using namespace translateLocally::models;
    std::vector<QString> keysSTR = {QString{"shortName"},
                                    QString{"modelName"},
                                    QString{"src"},
                                    QString{"trg"},
                                    QString("trgTag"),
                                    QString{"type"},
                                    QString("url"),
                                    QString("repository"),
                                    QString{"checksum"}};
    std::vector<QString> keysINT{QString("version"), QString("API")};
    
    QStringList missingKeys;

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
    // Note: this has to happen after all the parsing bits.
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

    // TODO do these checks for more keys that we really need
    if (obj.value("shortName").toString().isEmpty())
        missingKeys << "shortName";

    if (type == Remote && obj.value("url").toString().isEmpty())
        missingKeys << "url";

    if (!missingKeys.isEmpty()) {
        if (error) *error = tr("Model info is missing keys: %1").arg(missingKeys.join(' '));
        return std::nullopt;
    }

    return std::make_optional(model);
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

            // Possible parse error, useful for debugging
            QString errorMsg;

            QJsonObject obj = getModelInfoJsonFromDir(current, &errorMsg);
            
            // We have a folder in our models directory that doesn't contain a model. This is ok.
            if (obj.empty())
                continue;

            auto model = parseModelInfo(obj, translateLocally::models::Local, &errorMsg);
            if (!model) {
                emit error(tr("Invalid json file: %1/model_info.json: %2").arg(current, errorMsg));
                continue;
            }

            model->path = current;

            readModelMetaFromDir(*model, current);
            
            insertLocalModel(*model);
        } else {
            // Check if this an existing archive
            if (f.completeSuffix() == QString("tar.gz")) {
                archives_.append(f.fileName());
            }
        }
    }

    updateAvailableModels();
}

bool ModelManager::readModelMetaFromDir(ModelMeta &model, QString dir) const {
    QFile metaFile(dir + "/modelMeta.json");
    if (!metaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Could not parse model meta file" << metaFile.fileName() << ": file cannot be opened for reading.\n"
                 << "The model is either in the current working directory or downloaded before metadata was added to translateLocally.";
        return false; // Cannot open file, might not exist
    }
    
    QByteArray bytes = metaFile.readAll();
    metaFile.close();
    
    // Parse the Json
    QJsonParseError error;
    QJsonDocument json = QJsonDocument::fromJson(bytes, &error);
    if (json.isNull()) {
        qDebug() << "Could not parse model meta file" << metaFile.fileName() << ":" << error.errorString();
        return false; // Broken meta file, probably 
    }
    
    QJsonObject obj = json.object();
    model.modelUrl = obj.value("modelUrl").toString();
    model.repositoryUrl = obj.value("repositoryUrl").toString();
    model.installedOn = QDateTime::fromString(obj.value("installedOn").toString(), Qt::ISODate);
    return true;
}

bool ModelManager::writeModelMetaToDir(ModelMeta const &model, QString dir) const {
    QJsonDocument json{QJsonObject{
        {"modelUrl", model.modelUrl},
        {"repositoryUrl", model.repositoryUrl},
        {"installedOn", model.installedOn.toString(Qt::ISODate)},
    }};

    QFile metaFile(dir + "/modelMeta.json");
    if (!metaFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Could not write model meta file" << metaFile.fileName() << ": file cannot be opened for writing";
        return false; // Cannot open file, might not exist
    }

    metaFile.write(json.toJson());
    metaFile.close();
    
    return true;
}

void ModelManager::startupLoad() {
    // Scan for shared models installed through the system package manager.
    // Those paths should only contain already-extracted models.
    // They should be considered read-only.
    for (const auto &sharedDir : QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QString("models"), QStandardPaths::LocateDirectory)) {
        scanForModels(sharedDir);
    }

    // Iterate over all files in the app's data folder and take note of available models and archives
    scanForModels(appDataDir_.absolutePath());
    // Also scan for models located in the app's config directory in previous versions
    scanForModels(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
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

    auto repos = settings_->repos();
    QSharedPointer<int> num_repos(new int(repos.size())); // Keep track of how many repos have been fetched
    for (QString url : repos.keys()) {
        isFetchingRemoteModels_ = true;
        emit fetchingRemoteModels();

        QNetworkRequest request(url);
        QNetworkReply *reply = network_->get(request);
        connect(reply, &QNetworkReply::finished, this, [=] {
            switch (reply->error()) {
                case QNetworkReply::NoError:
                    parseRemoteModels(QJsonDocument::fromJson(reply->readAll()).object(), url);
                    break;
                default:
                    QString errstr = QString("Error fetching remote repository: ") + url +
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

void ModelManager::parseRemoteModels(QJsonObject obj, QString repositoryUrl) {
    using namespace translateLocally::models;
    
    QString errorMsg;
    bool empty = true;
    size_t i = 0;
    for (auto&& arrobj : obj["models"].toArray()) {
        ++i;
        empty = false;
        QJsonObject obj = arrobj.toObject();
        auto remoteModel = parseModelInfo(obj, Remote, &errorMsg);
        if (!remoteModel) {
            qDebug() << QString("Error while parsing model %1 of %2: %3").arg(i).arg(repositoryUrl, errorMsg);
            continue;
        }
        remoteModel->repositoryUrl = repositoryUrl;
        if (!remoteModels_.contains(*remoteModel)) { // This costs O(n). Not happy, is there a better way?
            remoteModels_.append(std::move(*remoteModel));
        }
    }
    if (empty) {
        emit error(tr("No models found in the repository at %1. Please double check that the repository address is correct.").arg(repositoryUrl));
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

std::optional<Repository> ModelManager::getRepository(const Model &model) const {
    auto repos = settings_->repos();
    auto it = repos.find(model.repositoryUrl);
    if (it == repos.end())
        return std::nullopt;
    return *it;
}

void ModelManager::updateAvailableModels() {
    // Reset the newModels.
    beginRemoveRows(QModelIndex(), localModels_.size(), localModels_.size() + newModels_.size() - 1);
    newModels_.clear();
    endRemoveRows();
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

    // We have changed available models, so insert remotes. Insert only once everything is processed
    beginInsertRows(QModelIndex(), localModels_.size(), localModels_.size() + newModels_.size() - 1);
    endInsertRows();

    emit localModelsChanged();
}

int ModelManager::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);

    return localModels_.size() + newModels_.size();
} 

int ModelManager::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);

    return 6;
}

QVariant ModelManager::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation != Qt::Horizontal)
        return QVariant();

    switch (section) {
        case Column::Source:
            return tr("Source", "Source language for the translation model.");
        case Column::Target:
            return tr("Target", "Target language for the translation model.");
        case Column::Type:
            return tr("Type", "The model type, affects speed.");
        case Column::Repo:
            return tr("Repository", "repository from which the translation model originated.");
        case Column::LocalVer:
            return tr("Version", "Version of the locally installed model.");
        case Column::RemoteVer:
            return tr("Available", "Version of the model that is available for download.");
        default:
            return QVariant();
    }
}

/**
 * Fake colour science! Dirty hack to give background colour a hint of the 
 * `hint` colour.
 * TODO: Improve this because although it is functional the colours are ugly.
 */
static QColor tint(QColor background, QColor hint) {
    auto bg = background.toHsv();
    auto fg = hint.toHsv();
    auto sat = (bg.saturation() + fg.saturation()) / 2.f;
    auto val = (bg.value() + fg.value()) / 2.f;
    return QColor::fromHsv(fg.hue(), sat, val, bg.alpha()).toRgb();
}

QVariant ModelManager::data(const QModelIndex &index, int role) const {
    Model model; // Make sure we have all local models before the remote ones
    if (index.row() < localModels_.size())
        model = localModels_[index.row()];
    else if (index.row() < localModels_.size() + newModels_.size())
        model = newModels_[index.row() - localModels_.size()];
    else if (index.row() >= localModels_.size() + newModels_.size()) // Return if we run overshoot the index.
        return QVariant();

    switch (role) {
        // Used for retrieving the underlying model data in a generic way that
        // survives e.g. QSortFilterProxyModel.
        case Qt::UserRole:
            return QVariant::fromValue(model);

        case Qt::DisplayRole: {
            switch (index.column()) {
                case Column::Source:
                    return model.src;
                case Column::Target:
                    return model.trg;
                case Column::Type:
                    return model.type;
                case Column::Repo: {
                    auto repo = getRepository(model);
                    return repo ? repo->name : model.getReportedRepo();
                }
                case Column::LocalVer:
                    if (model.isLocal()) {
                        return model.localversion;
                    } else {
                        return QVariant();
                    }
                case Column::RemoteVer:
                    if (model.isRemote()) {
                        return model.remoteversion;
                    } else {
                        return QVariant();
                    }
                default:
                    return QVariant();
            }
        }

        case Qt::TextAlignmentRole:
            switch (index.column()) {
                case Column::LocalVer:
                case Column::RemoteVer:
                    // @TODO figure out how to compile combined flag as below. 
                    // Error is "can't convert the result to QVariant."
                    // return Qt::AlignRight | Qt::AlignBaseline;
                    return Qt::AlignCenter;
                default:
                    return QVariant();
            }

        // TODO: should this colour mixing even be here? It feels odd to
        // have code for model management but also row colouring in the
        // same class.
        case Qt::BackgroundRole:
            if (model.isLocal()) {
                auto background = QApplication::style()->standardPalette().color(QPalette::Base);
                return tint(background, QColor(0x0, 0xFF, 0x0));
            } else {
                return QVariant();
            }
        
        case Qt::ForegroundRole:
            if (model.outdated()) {
                auto background = QApplication::style()->standardPalette().color(QPalette::Base);
                return tint(background, QColor(0xFF, 0x0, 0x0));
            } else {
                return QVariant();
            }

        default:
            return QVariant();
    }
}
