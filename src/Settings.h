#pragma once
#include <QObject>
#include <QSettings>
#include "types.h"

/**
 * Settings:
 * Class that houses all the invidividual settings. Also responsible
 * for providing the QSettings instance used by individidual settings.
 */
class Settings;


/**
 * Setting:
 * Type-independent base class for each setting. Separate from the
 * templated Setting<T> class because Q_OBJECT + templates does not work.
 * This class is the interface referenced elsewhere when connecting to the
 * `valueChanged` signal.
 */
class Setting : public QObject {
    Q_OBJECT

public:
    enum Behavior {
        AlwaysEmit,
        EmitWhenChanged
    };

protected:
    void emitValueChanged(QString name, QVariant value);

signals:
    void valueChanged(QString name, QVariant value);
};


/**
 * SettingImpl<T>:
 * Class that represents an individual setting. Similar to QProperty in Qt6.
 */
template <typename T>
class SettingImpl : public Setting {
private:
    QString name_;
    T value_;

public:
    SettingImpl(QSettings &backing, QString name, T defaultValue = T())
    : name_(name)
    , value_(backing.value(name, defaultValue).template value<T>()) {
        // When the value changes, also store it in QSettings
        connect(this, &Setting::valueChanged, &backing, &QSettings::setValue);
    }

    T value() const {
        return value_;
    }

    void setValue(T value, Behavior behavior = EmitWhenChanged) {
        // Don't emit when the value doesn't change
        if (behavior == EmitWhenChanged && value == value_)
            return;

        value_ = value;
        emitValueChanged(name_, value_);
    }

    // Alias for value() to make Settings make look more like a normal Qt class.
    T operator()() const {
        return value();
    }
};


class Settings : public QObject  {
    Q_OBJECT
private:
    QSettings backing_;

public:
    Settings(QObject *parent = nullptr);
    // For easy passing through of marian-related settings
    translateLocally::marianSettings marianSettings() const;

    SettingImpl<bool> translateImmediately;
    SettingImpl<QString> translationModel;
    SettingImpl<unsigned int> cores;
    SettingImpl<unsigned int> workspace;
    SettingImpl<Qt::Orientation> splitOrientation;
    SettingImpl<QByteArray> windowGeometry;
};
