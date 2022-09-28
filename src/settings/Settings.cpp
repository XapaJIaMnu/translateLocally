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
, alignmentColor(backing_, "alignment_color", QColor(0xED, 0xD4, 0x00))
, syncScrolling(backing_, "sync_scrolling", true)
, windowGeometry(backing_, "window_geometry")
, cacheTranslations(backing_, "cache_translations", true)
, repos(backing_, "newrepos", QMap<QString, translateLocally::Repository>{{translateLocally::kDefaultRepositoryURL, translateLocally::Repository{
                                                                                 translateLocally::kDefaultRepositoryName,
                                                                                 translateLocally::kDefaultRepositoryURL,
                                                                                 true
                                                                             }}})
, nativeMessagingClients(backing_, "native_messaging_clients", {
    // Firefox browser extension: https://github.com/jelmervdl/firefox-translations (unlisted & public)
    "{c9cdf885-0431-4eed-8e18-967b1758c951}",
    "{2fa36771-561b-452c-b6c3-7486f42c25ae}"
}) {
    //
}

translateLocally::marianSettings Settings::marianSettings() const {
    return {
        cores.value(),
        workspace.value(),
        cacheTranslations.value()
    };
}
