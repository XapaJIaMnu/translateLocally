#include "Settings.h"
#include <thread>

Settings::Settings(QObject *parent)
: QObject(parent)
, settings_(QSettings::NativeFormat, QSettings::UserScope, "translateLocally", "translateLocally")
, translate_immediately_(settings_.value("translate_immediately", true).toBool())
, translation_model_(settings_.value("translation_model").toString())
, cpu_cores_(settings_.value("cpu_cores", std::thread::hardware_concurrency()).toUInt())
, workspace_(settings_.value("workspace", 128).toUInt()) {
    bind("translate_immediately", &Settings::translateImmediatelyChanged);
    bind("translation_model", &Settings::translationModelChanged);
    bind("cpu_cores", &Settings::coresChanged);
    bind("workspace", &Settings::workspaceChanged);
}

translateLocally::marianSettings Settings::marianSettings() const {
    return {
        cores(),
        workspace()
    };
}
