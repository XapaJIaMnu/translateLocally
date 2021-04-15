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
            QDir().mkdir(configDir_.absolutePath());
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
    return QString("");
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
                QFile modelInfoFile(current + "/model_info.json");
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
                    modelDir model = {current, modelName, shortName, type};
                    // Push it onto the list of models
                    models_.push_back(model);
                } else {
                    std::cerr << "Failed to open file: " << (it.next() + "/model_info.json").toStdString() << std::endl; //@TODO popup error
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

void ModelManager::loadSettings() {

}
