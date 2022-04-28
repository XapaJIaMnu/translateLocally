#include "RepoManager.h"

RepoManager::RepoManager(QObject * parent)
: QObject(parent) {
    load({});
}

QList<Repository> RepoManager::getRepos() const {
    return repositories_.values();
}

void RepoManager::load(QList<QStringList> data) {
    repositories_.clear();

    repositories_.insert(kDefaultRepositoryURL, {kDefaultRepositoryName,kDefaultRepositoryURL, true});
    
    for (auto &&pair : data) {
        // Future proofing: make sure we don't load repositories that already exist
        // as default repositories.
        if (repositories_.contains(pair.first()))
            continue;

        repositories_.insert(pair.first(), {pair.first(), pair.back(), false});
    }
}

QString RepoManager::getName(QString url) const {
    auto it = repositories_.find(url);
    if (it != repositories_.end())
        return it->name;
    
    // TODO: come up with a more friendly name. E.g. use the shortest possible
    // unique prefix, e.g. the domain if there is no other repo with the same
    // domain.
    return url;
}
