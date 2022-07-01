#pragma once
#include <stddef.h>
#include <QString>

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
};

} // namespace translateLocally
