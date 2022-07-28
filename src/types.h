#pragma once
#include <stddef.h>
#include <QString>
#include <QVariant>

namespace translateLocally {

struct marianSettings {
    size_t cpu_threads;
    size_t workspace;
    bool translation_cache;
};

struct Repository {
    QString name;
    QString url;
    bool isDefault;

    inline friend QDataStream &operator<<(QDataStream &out, const Repository &repo) {
        out << repo.name << repo.url << QVariant(repo.isDefault).toString();
        return out;
    }

    inline friend QDataStream &operator>>(QDataStream &in, Repository &repo) {
        in >> repo.name;
        in >> repo.url;
        QString isDefault;
        in >> isDefault;
        repo.isDefault = QVariant(isDefault).toBool();
        return in;
    }

    inline bool operator==(const Repository& rhs) const {
        return (name == rhs.name) && (url == rhs.url) && (isDefault == rhs.isDefault);
    }
};

} // namespace translateLocally
