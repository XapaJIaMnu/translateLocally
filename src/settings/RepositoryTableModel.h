#pragma once
#include <QAbstractTableModel>
#include <QSet>
#include "settings/Settings.h"
#include "inventory/RepoManager.h"

class RepositoryTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    RepositoryTableModel(QObject * parent);
    
    bool canRemove(QModelIndex index) const;
    void insert(QString url, QString name);
    void removeRow(int index, QModelIndex const &parent = QModelIndex());
    void removeRows(QList<QModelIndex> rows);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    /**
     * @Brief Fills list using data from Setting.
     */
    void load(QList<QStringList> stored);

    /**
     * @Brief Dumps list in a format that can be stored in Settings.
     */
    QList<QStringList> dump() const;

    enum Column {
        Name,
        URL
    };

    Q_ENUM(Column);

signals:
    void warning(QString warn);
private:
    Repository* findByUrl(QString url);
    QList<Repository> repositories_;
    QSet<QString> urls_;
};
