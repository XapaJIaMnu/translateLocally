#include "Settings.h"
#include "constants.h"
#include <thread>

void Setting::emitValueChanged(QString name, QVariant value) {
    emit valueChanged(name, value);
}

Settings::Settings(QObject *parent)
: QObject(parent)
, backing_(QSettings::NativeFormat, QSettings::UserScope, "translateLocally", "translateLocally")
, translateImmediately(backing_, "translate_immediately", true)
, translationModel(backing_, "translation_model", "")
, cores(backing_, "cpu_cores", std::thread::hardware_concurrency())
, workspace(backing_, "workspace", 128)
, splitOrientation(backing_, "split", Qt::Vertical)
, showAlignment(backing_, "show_alignment", false)
, alignmentColor(backing_, "alignment_color", QColor("#EDD400"))
, syncScrolling(backing_, "sync_scrolling", true)
, windowGeometry(backing_, "window_geometry")
, cacheTranslations(backing_, "cache_translations", true)
, externalRepos(backing_, "external_repos", QList<QStringList>()) {
    //
}

translateLocally::marianSettings Settings::marianSettings() const {
    return {
        cores.value(),
        workspace.value(),
        cacheTranslations.value()
    };
}

QMap<QString,translateLocally::Repository> Settings::repos() const {
    QMap<QString,translateLocally::Repository> repositories;

    repositories.insert(translateLocally::kDefaultRepositoryURL, {
        translateLocally::kDefaultRepositoryName,
        translateLocally::kDefaultRepositoryURL,
        true
    });
    
    for (auto &&pair : externalRepos()) {
        // Future proofing: make sure we don't load repositories that already exist
        // as default repositories.
        if (repositories.contains(pair.back()))
            continue;

        repositories.insert(pair.back(), {pair.first(), pair.back(), false});
    }

    return repositories;
}