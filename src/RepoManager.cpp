#include "RepoManager.h"

RepoManager::RepoManager(QObject * parent, Settings * settings) : QAbstractTableModel(parent)
    , settings_(settings) {}

QStringList RepoManager::getRepos() {
    QStringList urls({kModelListUrl});
    for (auto&& nameAndUrl : settings_->externalRepos.value()) {
        urls.append(nameAndUrl.back());
    }
    return urls;
}

void RepoManager::insert(QStringList new_repo) {
    int position = settings_->externalRepos.value().size() + 1;
    beginInsertRows(QModelIndex(),position, position);
    settings_->externalRepos.appendToValue(new_repo);
    endInsertRows();
}

void RepoManager::remove(int index) {
    beginRemoveRows(QModelIndex(), index + 1, index + 1); // Account for the builtin repo
    settings_->externalRepos.removeFromValue(index);
    endRemoveRows();
}

int RepoManager::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return settings_->externalRepos.value().size() + 1; // The first item is hardcoded
}

int RepoManager::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);

    return 2;
}

QVariant RepoManager::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation != Qt::Horizontal)
        return QVariant();

    switch (section) {
        case RepoColumn::RepoName:
            return tr("Name", "name of the repository");
        case RepoColumn::URL:
            return tr("Repository", "url to the repository's model.json");
        default:
            return QVariant();
    }
}

QVariant RepoManager::data(const QModelIndex &index, int role) const {
    if (index.row() >= settings_->externalRepos.value().size() + 1)
        return QVariant();

    QStringList repo;
    if (index.row() == 0) {
        repo = QStringList{"Bergamot", kModelListUrl}; // Hardcoded default repo
    } else {
        repo = settings_->externalRepos.value().at(index.row() - 1);
    }

    if (role == Qt::UserRole) // ??
        return repo;

    switch (index.column()) {
        case RepoColumn::RepoName:
            switch (role) {
                case Qt::DisplayRole:
                    return repo.front();
                default:
                    return QVariant();
            }

        case RepoColumn::URL:
            switch (role) {
                case Qt::DisplayRole:
                    return repo.back();
                default:
                    return QVariant();
            }

    }

    return QVariant();
}
