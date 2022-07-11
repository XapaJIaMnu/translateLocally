#pragma once
#include <QAbstractTableModel>
#include <QSet>
#include "settings/Settings.h"
#include "types.h"

class RepositoryTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    RepositoryTableModel(QObject * parent);
    
    bool canRemove(QModelIndex index) const;
    void insert(QString name, QString url);
    void removeRow(int index, QModelIndex const &parent = QModelIndex());
    void removeRows(QList<QModelIndex> rows);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    /**
     * @Brief Fills list using data from Setting.
     */
    void load(QMap<QString, translateLocally::Repository> data);

    /**
     * @Brief Dumps list in a format that can be stored in Settings.
     */
    QMap<QString, translateLocally::Repository> dump() const;

    enum Column {
        Name,
        URL
    };

    Q_ENUM(Column);

signals:
    void warning(QString warn);
private:
    /**
     * @brief getKey Gets the key (URL) of the current row.
     * @param rowindex Index of the current row
     * @return The URL/key for the repository QMap
     */
    QString getKey(int rowindex) const;
    QMap<QString, translateLocally::Repository> repositories_;
};
