#ifndef MODELMANAGER_H
#define MODELMANAGER_H
#include <QSettings>
#include <QAbstractTableModel>
#include <QDir>
#include <QList>

struct modelDir {
    QString path; // This is full path to the directory
    QString name;
    QString shortName;
    QString type;
};

Q_DECLARE_METATYPE(modelDir)

class ModelManager : public QAbstractTableModel {
        Q_OBJECT
public:
    ModelManager(QObject *parent);
    void loadSettings();
    void writeModel(QString filename, QByteArray data);

    virtual QVariant data(QModelIndex const &, int role = Qt::DisplayRole) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    virtual int columnCount(QModelIndex const & = QModelIndex()) const;
    virtual int rowCount(QModelIndex const & = QModelIndex()) const;

    QSettings qset_;
    QDir configDir_;

    QStringList archives_; // Only archive name, not full path
    QList<modelDir> models_;

    static const int kColumnName;
    static const int kColumnShortName;
    static const int kColumnPathName;
    static const int kColumnType;
    
private:
    void startupLoad();
    void scanForModels(QString path);
    void extractTarGz(QString filename);
    modelDir parseModelInfo(QString path);

signals:
    void newModelAdded(int index);
    void error(QString);

};

#endif // MODELMANAGER_H
