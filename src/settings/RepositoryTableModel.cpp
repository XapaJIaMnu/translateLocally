#include "RepositoryTableModel.h"

RepositoryTableModel::RepositoryTableModel(QObject * parent)
: QAbstractTableModel(parent) {
    load({});
}

QList<QStringList> RepositoryTableModel::dump() const {
    QList<QStringList> out;
    for (auto &&repo : repositories_) {
        if (repo.isDefault) continue;
        out << QStringList{repo.name, repo.url};
    }
    return out;
}

void RepositoryTableModel::load(QList<QStringList> data) {
    beginRemoveRows(QModelIndex(), 0, repositories_.size());
    repositories_.clear();
    urls_.clear();
    endRemoveRows();

    repositories_ << Repository{kDefaultRepositoryName,kDefaultRepositoryURL, true};
    urls_.insert(kDefaultRepositoryURL);
    
    for (auto &&pair : data) {
        // Future proofing: make sure we don't load repositories that already exist
        // as default repositories.

        if (urls_.contains(pair.first()))
            continue;

        repositories_ << Repository{pair.first(), pair.back(), false};
        urls_.insert(pair.first());
    }
        
    // TODO: I'm lying here, it has already happened. Will Qt be okay with that?
    beginInsertRows(QModelIndex(), 0, repositories_.size());
    endInsertRows();
}

bool RepositoryTableModel::canRemove(QModelIndex index) const {
    return !repositories_.at(index.row()).isDefault;
}

void RepositoryTableModel::insert(QString url, QString name) {
    if (urls_.contains(url)) {
        // TODO: update name instead?
        emit warning("This repository is already in the list.");
        return;
    }

    int position = repositories_.size();
    beginInsertRows(QModelIndex(),position, position);
    repositories_ << Repository{name, url, false};
    urls_.insert(url);
    endInsertRows();
}

void RepositoryTableModel::removeRow(int index, QModelIndex const &parent) {
    Q_UNUSED(parent);
    beginRemoveRows(QModelIndex(), index, index);
    urls_.remove(repositories_.at(index).url);
    repositories_.remove(index);
    endRemoveRows();
}

void RepositoryTableModel::removeRows(QList<QModelIndex> rows) {
    // If we delete multiple repositories, indexes will change during deletion.
    // Instead make a list of urls (which are unique) to delete.
    QSet<QString> toDelete;
    int first = rowCount(), last = 0;

    for (auto &&index : rows) {
        if (!canRemove(index)) {
            emit warning("Unable to remove the builtin repository.");
            continue; // TODO this could be trouble if it appeared in the middle of the unremovable rows
        }

        if (index.row() < first) first = index.row();
        if (index.row() > last) last = index.row();
        toDelete << repositories_.at(index.row()).url;
    }

    beginRemoveRows(QModelIndex(), first, last);
    for (int i = first; i < last;) {
        if (toDelete.contains(repositories_.at(i).url)) {
            urls_.remove(repositories_.at(i).url);
            repositories_.remove(i);
            --last; // No ++i since we removed i (so next round row[i] will be the next one) but our list has become 1 shorter.
        } else {
            ++i; // E.g. default repo we need to skip
        }
    }
    endRemoveRows();
}

int RepositoryTableModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return repositories_.size();
}

int RepositoryTableModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return 2;
}

QVariant RepositoryTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation != Qt::Horizontal)
        return QVariant();

    switch (section) {
        case Column::Name:
            return tr("Name", "name of the repository");
        case Column::URL:
            return tr("Repository", "url to the repository's model.json");
        default:
            return QVariant();
    }
}

QVariant RepositoryTableModel::data(const QModelIndex &index, int role) const {
    if (index.row() >= repositories_.size())
        return QVariant();

    Repository const *repo = &repositories_.at(index.row());

    switch (index.column()) {
        case Column::Name:
            switch (role) {
                case Qt::DisplayRole:
                    return repo->name;
                default:
                    return QVariant();
            }

        case Column::URL:
            switch (role) {
                case Qt::DisplayRole:
                    return repo->url;
                default:
                    return QVariant();
            }

    }

    return QVariant();
}
