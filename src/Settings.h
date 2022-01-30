#pragma once
#include <type_traits>
#include <QObject>
#include <QStringList>
#include <QSettings>
#include <QColor>
#include <QMessageBox>
#include "types.h"

Q_DECLARE_METATYPE(QList<QStringList>);

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
    // Plumbing to the mainwindow for proper error management is a lot of work since this class is
    // quite down the rabbit hole. This error is only meant to be seen by a developer anyways, not
    // by the end user and it is a sanity check to whether the API is used correctly.
    inline void error(QString err) {
        QMessageBox::critical(this, tr("An error occurred"), err);
    }

public:
    SettingImpl(QSettings &backing, QString name, T defaultValue = T())
    : name_(name) {
        if constexpr (std::is_same_v<QList<QStringList>, T>) {
            value_ = backing.value(name, QVariant::fromValue<QList<QStringList>>(QList<QStringList>())).value<QList<QStringList>>();
        } else {
            value_ = backing.value(name, defaultValue).template value<T>();
        }
        // When the value changes, also store it in QSettings
        connect(this, &Setting::valueChanged, &backing, &QSettings::setValue);
    }

    T value() const {
        return value_;
    }

    void setValue(T value, Behavior behavior = EmitWhenChanged) {
        if  constexpr (std::is_same_v<QList<QStringList>, T>) {
            // Don't emit when the value doesn't change
            if (behavior == EmitWhenChanged && value == value_)
                return;

            value_ = value;
            emitValueChanged(name_, QVariant::fromValue<QList<QStringList>>(value_));
        } else {
            // Don't emit when the value doesn't change
            if (behavior == EmitWhenChanged && value == value_)
                return;

            value_ = value;
            emitValueChanged(name_, value_);
        }
    }

    void appendToValue(QStringList entry, Behavior behavior = EmitWhenChanged) {
        if  constexpr (std::is_same_v<QList<QStringList>, T>) {
            value_.append(entry);
            emitValueChanged(name_, QVariant::fromValue<QList<QStringList>>(value_));
        } else {
            error(QString(tr("Attempted to append a setting entry to a non-appendable type: ")) + QString(typeid(T).name()));
        }
    }

    void removeFromValue(int index, Behavior behavior = EmitWhenChanged) {
        if  constexpr (std::is_same_v<QList<QStringList>, T>) {
            value_.removeAt(index);
            emitValueChanged(name_, QVariant::fromValue<QList<QStringList>>(value_));
        } else {
            error(QString(tr("Attempted to remove a setting entry to a non-removable type: ")) + QString(typeid(T).name()));
        }
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
    SettingImpl<bool> showAlignment;
    SettingImpl<QColor> alignmentColor;
    SettingImpl<bool> syncScrolling;
    SettingImpl<QByteArray> windowGeometry;
    SettingImpl<bool> cacheTranslations;
    SettingImpl<QList<QStringList>> externalRepos; // Format is {{name, repo}, {name, repo}...}. There are more suitable formats, but this one actually is a QVariant
};
