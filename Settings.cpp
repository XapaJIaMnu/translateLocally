#include "Settings.h"
#include <thread>


Settings::Settings(QObject *parent)
: QObject(parent)
, settings_(QSettings::NativeFormat, QSettings::UserScope, "translateLocally", "translateLocally")
, cpu_cores_(settings_.value("cpu_cores", std::thread::hardware_concurrency()).toUInt())
, workspace_(settings_.value("workspace", 128).toUInt()) {
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
