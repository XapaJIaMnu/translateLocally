#include "Settings.h"
#include <thread>


Settings::Settings(QObject *parent)
: QObject(parent)
, settings_(QSettings::NativeFormat, QSettings::UserScope, "translateLocally", "translateLocally")
, translate_immediately_(settings_.value("translate_immediately", true).toBool())
, translation_model_(settings_.value("translation_model").toString())
, cpu_cores_(settings_.value("cpu_cores", std::thread::hardware_concurrency()).toUInt())
, workspace_(settings_.value("workspace", 128).toUInt()) {
    connect(this, &Settings::translateImmediatelyChanged, this, [&](bool enable) {
        settings_.setValue("translate_immediately", enable);
    });

    connect(this, &Settings::translationModelChanged, this, [&](QString path) {
        settings_.setValue("translation_model", path);
    });

    connect(this, &Settings::coresChanged, this, [&](unsigned int cores) {
        settings_.setValue("cpu_cores", cores);
    });
    
    connect(this, &Settings::workspaceChanged, this, [&](unsigned int workspace) {
        settings_.setValue("workspace", workspace);
    });
}

translateLocally::marianSettings Settings::marianSettings() const {
    return {
        cpu_cores_,
        workspace_
    };
}
