#pragma once
#include <stddef.h>
namespace translateLocally {

struct marianSettings {
    size_t cpu_threads;
    size_t workspace;
    bool translation_cache;
};

} // namespace translateLocally
