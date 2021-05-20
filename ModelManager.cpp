#include "ModelManager.h"
#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <iostream>
// libarchive
#include <archive.h>
#include <archive_entry.h>


ModelManager::ModelManager(QObject *parent)
    : QAbstractTableModel(parent)
    , nam_(new QNetworkAccessManager(this))
    , qset_(QSettings::NativeFormat, QSettings::UserScope, "translateLocally", "translateLocally")
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
    // @TODO read settings:
    settings_ = translateLocally::marianSettings();
}

Model ModelManager::writeModel(QString filename, QByteArray data) {
    QString fullpath(configDir_.absolutePath() + QString("/") + filename);
    QSaveFile file(fullpath);
    Model newmodel;
    
    bool openReady = file.open(QIODevice::WriteOnly);
    if (!openReady) {
        emit error(QString("Failed to open file: " + fullpath));
        return newmodel;
    }
    file.write(data);
    bool commitReady = file.commit();
    if (!commitReady) {
        emit error(QString("Failed to write to file: " + fullpath + " . Did you run out of disk space?"));
        return newmodel;
    }
    extractTarGz(filename);
    // Add the new tar to the archives list
    archives_.push_back(filename);

    // Add the model to the local models and emit a signal with its index
    QString newModelDirName = filename.split(".tar.gz")[0];
    QJsonObject obj = getModelInfoJsonFromDir(configDir_.absolutePath() + QString("/") + newModelDirName);

    // The function above should have added the path variable. it will error message if it fails.
    if (obj.find("path") == obj.end()) {
        emit error(QString("Failed to find, open or parse the model_info.json for the newly dowloaded " + filename));
        return newmodel;
    }
    newmodel = parseModelInfo(obj);

    beginInsertRows(QModelIndex(), localModels_.size(), localModels_.size());
    localModels_.append(newmodel);
    endInsertRows();

    return newmodel;
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
            emit error("Failed to open json config file: " + modelInfo.absoluteFilePath());
            return QJsonObject();
        }
    } else {
        // Model info doesn't exist or a configuration file is not found. Handle the error elsewhere.
        return QJsonObject();
    }
}

Model ModelManager::parseModelInfo(QJsonObject& obj, bool local) {
    std::vector<QString> keysSTR = {QString{"shortName"},
                                    QString{"modelName"},
                                    QString{"src"},
                                    QString{"trg"},
                                    QString{"type"}};
    std::vector<QString> keysFLT{QString("version"), QString("API")};
    QString criticalKey = local ? QString("path") : QString("url");

    Model model = {};
    // Non critical keys. Some of them might be missing from old model versions but we don't care
    for (auto&& key : keysSTR) {
        auto iter = obj.find(key);
        if (iter != obj.end()) {
            model.set(key, iter.value().toString());
        } else {
            model.set(key, "");
        }
    }

    // Float Keys depend on whether we have a local or a remote model
    // Non critical if missing due to older file name
    for (auto&& key : keysFLT) {
        QString keyname = local ? "local" + key : "remote" + key;
        auto iter = obj.find(key);
        if (iter != obj.end()) {
            model.set(keyname, (float)iter.value().toDouble());
        } else {
            model.set(keyname, "");
        }
    }

    // Critical key. If this key is missing the json is completely invalid and needs to be discarded
    // it's either the path to the model or the url to its download location
    auto iter = obj.find(criticalKey);
    if (iter != obj.end()) {
        model.set(criticalKey, iter.value().toString());
    } else {
        emit error ("The json file provided is missing " + criticalKey + " or is corrupted. Please redownload the model.\
                     If the path variable is missing, it is added automatically, so please file a bug report at: https://github.com/XapaJIaMnu/translateLocally/issues");
    }

    return model;
}

void ModelManager::scanForModels(QString path) {
    //Iterate over all files in the folder and take note of available models and archives
    //@TODO currently, archives can only be extracted from the config dir
    QDirIterator it(path, QDir::NoFilter);
    QList<Model> models;
    while (it.hasNext()) {
        QString current = it.next();
        QFileInfo f(current);
        if (f.isDir()) {
            QJsonObject obj = getModelInfoJsonFromDir(current);
            if (!obj.empty()) {
                Model model = parseModelInfo(obj);
                if (model.path != "") {
                    models.append(model);
                } else {
                    emit error("Corrupted json file: " + current + "/model_info.json" + " . Delete or redownload.");
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

    beginInsertRows(QModelIndex(), localModels_.size(), localModels_.size() + models.size() - 1);
    localModels_ += models;
    endInsertRows();
}

void ModelManager::startupLoad() {
    //Iterate over all files in the config folder and take note of available models and archives
    scanForModels(configDir_.absolutePath());
    scanForModels(QDir::current().path()); // Scan the current directory for models. @TODO archives found in this folder would not be used
}
// Adapted from https://github.com/libarchive/libarchive/blob/master/examples/untar.c#L136
void ModelManager::extractTarGz(QString filein) {
    // Since I can't figure out (or can be bothered to) how libarchive extracts files in a different directory, I'll just temporary change
    // the working directory and then change it back. Sue me.
    QString currentpath = QDir::current().path();
    if (currentpath != configDir_.absolutePath()) {
        bool pathChanged = QDir::setCurrent(configDir_.absolutePath());
        if (!pathChanged) {
            emit error(QString("Failed to change path to the configuration directory ") + configDir_.absolutePath() + " " + filein + " won't be extracted.");
            return;
        }
    }
    // Extraction code begins. Some minor functions to tie in what's missing from untar.c
    int flags = ARCHIVE_EXTRACT_TIME;
    std::string fileinStr = filein.toStdString();
    const char * filename = fileinStr.c_str();
    int do_extract = 1;
    int verbose = 0;

    // Lambdas
    auto msg = [&](const char *m) {
        emit error (QString(m));
    };

    auto warn = [&](const char *f, const char *m) {
        emit error("Warning: " + QString(f) + " " + QString(m));
    };

    auto fail = [&](const char *f, const char *m, int r) {
        emit error("Critical: " + QString(f) + " " + QString(m) + " libarchive wanted to exit with exit code: " + QString::fromStdString(std::to_string(r)) + ". Archive extraction has most likely failed.");
    };

    auto copy_data = [=](struct archive *ar, struct archive *aw) {
        int r;
        const void *buff;
        size_t size;
#if ARCHIVE_VERSION_NUMBER >= 3000000
        int64_t offset;
#else
        off_t offset;
#endif

        for (;;) {
            r = archive_read_data_block(ar, &buff, &size, &offset);
            if (r == ARCHIVE_EOF)
                return (ARCHIVE_OK);
            if (r != ARCHIVE_OK)
                return (r);
            r = archive_write_data_block(aw, buff, size, offset);
            if (r != ARCHIVE_OK) {
                warn("archive_write_data_block()",
                     archive_error_string(aw));
                return (r);
            }
        }
    };

    //Actual code

    struct archive *a;
    struct archive *ext;
    struct archive_entry *entry;
    int r;

    a = archive_read_new();
    ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, flags);
    /*
         * Note: archive_write_disk_set_standard_lookup() is useful
         * here, but it requires library routines that can add 500k or
         * more to a static executable.
         */
    archive_read_support_format_tar(a);
    archive_read_support_filter_gzip(a);
    /*
         * On my system, enabling other archive formats adds 20k-30k
         * each.  Enabling gzip decompression adds about 20k.
         * Enabling bzip2 is more expensive because the libbz2 library
         * isn't very well factored.
         */
    if (filename != NULL && strcmp(filename, "-") == 0)
        filename = NULL;
    if ((r = archive_read_open_filename(a, filename, 10240)))
        fail("archive_read_open_filename()",
             archive_error_string(a), r);
    for (;;) {
        r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF)
            break;
        if (r != ARCHIVE_OK)
            fail("archive_read_next_header()",
                 archive_error_string(a), 1);
        if (verbose && do_extract)
            msg("x ");
        if (verbose || !do_extract)
            msg(archive_entry_pathname(entry));
        if (do_extract) {
            r = archive_write_header(ext, entry);
            if (r != ARCHIVE_OK)
                warn("archive_write_header()",
                     archive_error_string(ext));
            else {
                copy_data(a, ext);
                r = archive_write_finish_entry(ext);
                if (r != ARCHIVE_OK)
                    fail("archive_write_finish_entry()",
                         archive_error_string(ext), 1);
            }

        }
        if (verbose || !do_extract)
            msg("\n");
    }
    archive_read_close(a);
    archive_read_free(a);

    archive_write_close(ext);
    archive_write_free(ext);

    // Extraction code ends, change path back
    if (currentpath != configDir_.absolutePath()) {
        bool pathChanged = QDir::setCurrent(currentpath);
        if (!pathChanged) {
            emit error(QString("Failed to change path to the current directory ") + currentpath );
            return;
        }
    }
}

void ModelManager::loadSettings() {

}

void ModelManager::fetchRemoteModels() {
    QUrl url("http://data.statmt.org/bergamot/models/models.json");
    QNetworkRequest request(url);
    QNetworkReply *reply = nam_->get(request);
    connect(reply, &QNetworkReply::finished, this, [&, reply]() {
        switch (reply->error()) {
            case QNetworkReply::NoError:
                parseRemoteModels(QJsonDocument::fromJson(reply->readAll()).object());
                emit fetchedRemoteModels();
                break;
            default:
                emit error(reply->errorString());
                break;
        }
        reply->deleteLater();
    });
}

void ModelManager::parseRemoteModels(QJsonObject obj) {
    beginRemoveRows(QModelIndex(),
        localModels_.size(),
        localModels_.size() + remoteModels_.size() - 1);
    remoteModels_.clear();
    endRemoveRows();

    QList<RemoteModel> models;
    for (auto&& arrobj : obj["models"].toArray()) {
        models.append(RemoteModel{
            arrobj.toObject()["name"].toString(),
            arrobj.toObject()["code"].toString(),
            arrobj.toObject()["url"].toString()
        });
    }

    beginInsertRows(QModelIndex(),
        localModels_.size(),
        localModels_.size() + models.size() - 1);
    remoteModels_ = models;
    endInsertRows();
}

QList<Model> ModelManager::installedModels() const {
    return localModels_;
}

QList<RemoteModel> ModelManager::remoteModels() const {
    return remoteModels_;
}

QList<RemoteModel> ModelManager::availableModels() const {
    QList<RemoteModel> filtered;
    for (auto &&model : remoteModels_) {
        bool installed = false;

        for (auto &&localModel : localModels_) {
            // TODO: matching by name might not be very robust
            if (localModel.modelName == model.name) {
                installed = true;
                break;
            }
        }

        if (!installed)
            filtered.append(model);
    }

    return filtered;
}

QVariant ModelManager::data(QModelIndex const &index, int role) const {
    if (index.row() <= localModels_.size()) {
        Model const &model = localModels_[index.row()];

        switch (role) {
            case Qt::UserRole:
                return QVariant::fromValue(model);
            case Qt::DisplayRole:
                switch (index.column()) {
                    case ModelManager::Column::ModelName:
                        return model.modelName;
                    case Column::ShortName:
                        return model.shortName;
                    case Column::PathName:
                        return model.path;
                    case Column::Type:
                        return model.type;
                    // Intentional fall-through for default
                }
            default:
                return QVariant();
        }
    } else if (index.row() - localModels_.size() <= remoteModels_.size()) {
        RemoteModel const &model = remoteModels_[index.row() - localModels_.size()];

        switch (role) {
            case Qt::UserRole:
                return QVariant::fromValue(model);
            case Qt::DisplayRole:
                switch (index.column()) {
                    case Column::ModelName:
                        return model.name;
                    case Column::ShortName:
                        return model.code;
                    case Column::PathName:
                        return model.url;
                    case Column::Type:
                        return QString();
                    // Intentional fall-through for default
                }
            default:
                return QVariant();
        }
    } else {
        return QVariant();
    }
}

QVariant ModelManager::headerData(int section, Qt::Orientation orientation, int role) const {
    Q_UNUSED(orientation);

    if (role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
        case Column::ModelName:
            return "Name";
        case Column::ShortName:
            return "Short name";
        case Column::PathName:
            return "Path";
        case Column::Type:
            return "Type";
        default:
            return QVariant();
    }
}

int ModelManager::columnCount(QModelIndex const &index) const {
    Q_UNUSED(index);

    return kColumnCount;
}

int ModelManager::rowCount(QModelIndex const &index) const {
    Q_UNUSED(index);

    return localModels_.size() + remoteModels_.size();
}

