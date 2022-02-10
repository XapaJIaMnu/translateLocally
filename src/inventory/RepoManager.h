#pragma once
#include <QAbstractTableModel>
#include "settings/Settings.h"

constexpr const char* kDefaultRepositoryName = "Bergamot";

constexpr const char* kDefaultRepositoryURL = "https://translatelocally.com/models.json";

class RepoManager : public QAbstractTableModel {
    Q_OBJECT
public:
    RepoManager(QObject * parent, Settings *);
    /**
     * @brief getRepos getsAll currently available repos
     * @return QStringList of URLs
     */
    QStringList getRepos();
    bool canRemove(QModelIndex index) const;
    void insert(QStringList new_model);
    void removeRow(int index, QModelIndex const &parent = QModelIndex());
    void removeRows(QList<QModelIndex> rows);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    enum RepoColumn {
        RepoName,
        URL
    };

    Q_ENUM(RepoColumn);
private:
    Settings * settings_;

};
