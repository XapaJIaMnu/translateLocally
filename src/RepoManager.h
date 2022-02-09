#pragma once
#include <QAbstractTableModel>
#include "Settings.h"

constexpr const char* kModelListUrl = "https://translatelocally.com/models.json";

class RepoManager : public QAbstractTableModel {
    Q_OBJECT
public:
    RepoManager(QObject * parent, Settings *);
    /**
     * @brief getRepos getsAll currently available repos
     * @return QStringList of URLs
     */
    QStringList getRepos();
    void insert(QStringList new_model);
    void remove(int index);

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
