#ifndef MODELMANAGER_H
#define MODELMANAGER_H
#include <QSettings>
#include <QObject>
#include <QDir>
#include <QList>

struct modelDir {
    QString path; // This is full path to the directory
    QString name;
    QString shortName;
    QString type;
};

class ModelManager : public QObject {
        Q_OBJECT
public:
    ModelManager(QObject *parent);
    void loadSettings();
    void writeModel(QString filename, QByteArray data);

    QSettings qset_;
    QDir configDir_;

    QStringList archives_; // Only archive name, not full path
    QList<modelDir> models_;
private:
    void startupLoad();
    void extractTarGz(QString filename);
    modelDir parseModelInfo(QString path);

signals:
    void newModelAdded(int index);
    void error(QString);

};

#endif // MODELMANAGER_H
