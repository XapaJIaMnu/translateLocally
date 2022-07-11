#include "RepositoryTableModel.h"
#include "types.h"
#include "constants.h"
#include <QSet>
#include <iostream>

// TODO resolve this namespace situation
using namespace translateLocally;

RepositoryTableModel::RepositoryTableModel(QObject * parent)
: QAbstractTableModel(parent) {
    load({});
}

QString RepositoryTableModel::getKey(int rowindex) const {
    QModelIndex myidx = this->createIndex(rowindex, 1); // Select the key, which is the URL
    return myidx.data().toString();
}

QMap<QString, translateLocally::Repository> RepositoryTableModel::dump() const {
    return repositories_;
}

void RepositoryTableModel::load(QMap<QString, translateLocally::Repository> data) {
    beginRemoveRows(QModelIndex(), 0, repositories_.size());
    repositories_.clear();
    endRemoveRows();

    beginInsertRows(QModelIndex(), 0, data.size());
    repositories_ = data;
    endInsertRows();
}

bool RepositoryTableModel::canRemove(QModelIndex index) const {
    QString mykey = getKey(index.row());
    return !repositories_[mykey].isDefault;
}

void RepositoryTableModel::insert(QString name, QString url) {
    if (repositories_.contains(url)) {
        // TODO: update name instead?
        emit warning("This repository is already in the list.");
        return;
    }

    int position = repositories_.size();
    beginInsertRows(QModelIndex(),position, position);
    repositories_.insert(url, Repository{name, url, false});
    endInsertRows();
}

void RepositoryTableModel::removeRow(int index, QModelIndex const &parent) {
    Q_UNUSED(parent);
    QString mykey = getKey(index);
    beginRemoveRows(QModelIndex(), index, index);
    repositories_.remove(mykey);
    endRemoveRows();
}

void RepositoryTableModel::removeRows(QList<QModelIndex> rows) {
    QList<QString> toRemoveKeys;
    int first = rowCount(), last = 0;
    for (auto&& idx : rows) {
        if (!canRemove(idx)) {
            emit warning("Unable to remove the builtin repository.");
            continue;
        }
        if (idx.row() < first) first = idx.row();
        if (idx.row() > last) last = idx.row();
        toRemoveKeys.push_back(getKey(idx.row()));
    }

    beginRemoveRows(QModelIndex(), first, last);
    for (auto&& key : toRemoveKeys) {
        repositories_.remove(key);
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
    if (!index.isValid() || index.row() >= repositories_.size() || role != Qt::DisplayRole)
        return QVariant();

    auto it = repositories_.cbegin();
    std::advance(it, index.row());
    Repository const& repo = it.value();

    switch (index.column()) {
        case Column::Name:
            switch (role) {
                case Qt::DisplayRole:
                    return repo.name;
                default:
                    return QVariant();
            }

        case Column::URL:
            switch (role) {
                case Qt::DisplayRole:
                    return repo.url;
                default:
                    return QVariant();
            }

    }

    return QVariant();
}
