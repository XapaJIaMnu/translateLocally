#include "ModelManager.h"
#include <filesystem>
#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonDocument>
#include <iostream>
// libarchive
#include <archive.h>
#include <archive_entry.h>

ModelManager::ModelManager(QObject *parent)
    : QObject(parent)
    , qset_(QSettings::NativeFormat, QSettings::UserScope, "translateLocally", "translateLocally"){
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
    std::cerr << "ConfigDir path is: " << configDir_.absolutePath().toStdString() << std::endl;
    startupLoad();
}

QString ModelManager::writeModel(QString filename, QByteArray data) {
    QString fullpath(configDir_.absolutePath() + QString("/") + filename);
    QSaveFile file(fullpath);
    bool openReady = file.open(QIODevice::WriteOnly);
    if (!openReady) {
        return QString("Failed to open file: " + fullpath);
    }
    file.write(data);
    bool commitReady = file.commit();
    if (!commitReady) {
        return QString("Failed to write to file: " + fullpath + " . Did you run out of disk space?");
    }
    extractTarGz(filename);
    // Add the new tar to the archives list
    archives_.push_back(filename);

    // Add the model to the local models and emit a signal with its index
    QString newModelDirName = filename.split(".tar.gz")[0];
    modelDir newmodel = parseModelInfo(configDir_.absolutePath() + QString("/") + newModelDirName);
    if (newmodel.path == QString("")) {
        return QString("Failed to parse the model_info.json for the newly dowloaded " + filename);
    }
    int new_idx = models_.size();
    models_.append(newmodel);
    emit newModelAdded(new_idx);
    return QString("");
}

modelDir ModelManager::parseModelInfo(QString path) {
    // Check if we can find a model_info.json in the directory. If so, record it as part of the model
    QFile modelInfoFile(path + "/model_info.json");
    bool isOpen = modelInfoFile.open(QIODevice::ReadOnly | QIODevice::Text);
    if (isOpen) {
        QByteArray bytes = modelInfoFile.readAll();
        modelInfoFile.close();
        // Parse the Json
        QJsonDocument jsonResponse = QJsonDocument::fromJson(bytes);
        QJsonObject obj = jsonResponse.object();
        QString modelName = obj["modelName"].toString();
        QString shortName = obj["shortName"].toString();
        QString type = obj["type"].toString();
        modelDir model = {path, modelName, shortName, type};
        // Push it onto the list of models
        return model;
    } else {
        std::cerr << "Failed to open file: " << (path + "/model_info.json").toStdString() << std::endl; //@TODO popup error
        return modelDir{"", "", "", ""}; // Invalid modelDir
    }
}

void ModelManager::startupLoad() {
    //Iterate over all files in the config folder and take note of available models and archives
    QDirIterator it(configDir_.absolutePath(), QDir::NoFilter);
    while (it.hasNext()) {
        QString current = it.next();
        QFileInfo f(current);
        if (f.isDir()) {
            // Check if we can find a model_info.json in the directory. If so, record it as part of the model
            QFileInfo modelInfo(current + "/model_info.json");
            if (modelInfo.exists()) {
                modelDir model = parseModelInfo(current);
                if (model.path != "") {
                    models_.append(model);
                }
            }
        } else {
            // Check if this an existing archive
            if (f.completeSuffix() == QString("tar.gz")) {
                archives_.append(f.fileName());
            }
        }
    }
}
// Adapted from https://github.com/libarchive/libarchive/blob/master/examples/untar.c#L136
void ModelManager::extractTarGz(QString filein) {
    // Since I can't figure out (or can be bothered to) how libarchive extracts files in a different directory, I'll just temporary change
    // the working directory and then change it back. Sue me.
    QString currentpath = QDir::current().path();
    if (currentpath != configDir_.absolutePath()) {
        bool pathChanged = QDir::setCurrent(configDir_.absolutePath());
        if (!pathChanged) {
            std::cerr << "Failed to change path to the configuration directory " << configDir_.absolutePath().toStdString() << std::endl; //@TODO connect to slot
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
    auto msg = [](const char *m) {
        std::cerr << m << std::endl; //@TODO connect to slot.
    };

    auto warn = [](const char *f, const char *m) {
        std::cerr << f << " failed " << m << std::endl; //@TODO connect to slot.
    };

    auto fail = [](const char *f, const char *m, int r) {
        std::cerr << f << " failed " << m << " libarchive wanted to exit with code " << r << " but we keep running! " << std::endl; //@TODO connect to slot.
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
            std::cerr << "Failed to change path to the configuration directory " << configDir_.absolutePath().toStdString() << std::endl; //@TODO connect to slot
            return;
        }
    }
}

void ModelManager::loadSettings() {

}
