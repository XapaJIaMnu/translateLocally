#include "Settings.h"
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
, windowGeometry(backing_, "window_geometry") {
    //
}

translateLocally::marianSettings Settings::marianSettings() const {
    return {
        cores.value(),
        workspace.value()
    };
}
