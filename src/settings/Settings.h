#pragma once
#include <type_traits>
#include <QObject>
#include <QStringList>
#include <QSettings>
#include <QColor>
#include <QSet>
#include "types.h"

Q_DECLARE_METATYPE(QList<QStringList>);

Q_DECLARE_METATYPE(QSet<QString>);

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
    QSettings &backing_;
    QString name_;
    QVariant default_;

public:
    SettingImpl(QSettings &backing, QString name, T defaultValue = T())
    : backing_(backing)
    , name_(name)
    , default_(QVariant::fromValue(defaultValue)) {
        //
    }

    T value() const {
        return backing_.value(name_, default_).template value<T>();
    }

    void setValue(T newValue, Behavior behavior = EmitWhenChanged) {
        if (behavior == EmitWhenChanged && newValue == value())
            return;

        backing_.setValue(name_, QVariant::fromValue(newValue));
        emitValueChanged(name_, backing_.value(name_)); // not using value() because we *want* the QVariant
    }

    // Here for compatibility. Will be removed once #102 is merged.
    template<typename Val>
    void appendToValue(Val newValue) {
        if constexpr (std::is_same_v<QList<Val>, T>) {
            QList<Val> values = value();
            values.append(newValue);
            setValue(values);
        }
    }

    // Here for compatibility. Will be removed once #102 is merged.
    template <typename Val>
    void removeFromValue(Val needle) {
        if constexpr (std::is_same_v<QList<Val>, T>) {
            QList<Val> values = value();
            values.removeAll(needle);
            setValue(values);
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
    SettingImpl<QSet<QString>> nativeMessagingClients;
};
