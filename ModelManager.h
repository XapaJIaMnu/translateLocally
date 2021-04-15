#ifndef MODELMANAGER_H
#define MODELMANAGER_H
#include <QSettings>
#include <QObject>
#include <QDir>
#include <QList>

struct modelDir {
    QString path;
    QString name;
    QString shortName;
    QString type;
};

class ModelManager : public QObject {
        Q_OBJECT
public:
    ModelManager(QObject *parent);
    void loadSettings();
    QString writeModel(QString filename, QByteArray data);

    QSettings qset_;
    QDir configDir_;

    QStringList archives_;
    QList<modelDir> models_;
private:
    void startupLoad();


};

#endif // MODELMANAGER_H
