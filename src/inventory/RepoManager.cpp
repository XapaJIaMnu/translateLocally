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

bool RepoManager::canRemove(QModelIndex index) const {
    return index.row() != 0; // First row is hardcoded
}

void RepoManager::insert(QStringList new_repo) {
    int position = settings_->externalRepos.value().size() + 1;
    beginInsertRows(QModelIndex(),position, position);
    settings_->externalRepos.appendToValue(new_repo);
    endInsertRows();
}

void RepoManager::removeRow(int index, QModelIndex const &parent) {
    Q_UNUSED(parent);
    beginRemoveRows(QModelIndex(), index, index);
    settings_->externalRepos.removeFromValue(index - 1); // Account for the builtin repo
    endRemoveRows();
}

void RepoManager::removeRows(QList<QModelIndex> rows) {
    // If we delete multiple repositories, indexes will change during deletion.
    // Instead make a list of items to be deleted and the remove them one by
    // one, every time recalculating the index.
    QList<QStringList> toDelete;
    int first = rowCount(), last = 0;

    for (auto &&index : rows) {
        if (!canRemove(index))
            continue;

        if (index.row() < first) first = index.row();
        if (index.row() > last) last = index.row();
        toDelete.append(settings_->externalRepos.value().at(index.row() - 1)); // Account for builtIn hardcoded repo that lives outside the settings.
    }

    beginRemoveRows(QModelIndex(), first, last);
    for (auto &&repo : toDelete)
        settings_->externalRepos.removeFromValue(settings_->externalRepos.value().indexOf(repo));
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

    if (role == Qt::UserRole) // Allow access to the underlying data through the UserRole role.
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
