#pragma once
#include <QObject>
#include <QSettings>
#include "types.h"


class Settings : public QObject 
{
    Q_OBJECT
    Q_PROPERTY(QString translationModel READ translationModel WRITE setTranslationModel NOTIFY translationModelChanged)
    Q_PROPERTY(unsigned int cores READ cores WRITE setCores NOTIFY coresChanged)
    Q_PROPERTY(unsigned int workspace READ workspace WRITE setWorkspace NOTIFY workspaceChanged)

private:
    QSettings settings_;
    QString translation_model_;
    unsigned int cpu_cores_;
    unsigned int workspace_;
public:
    Settings(QObject *parent = nullptr);
    translateLocally::marianSettings marianSettings() const;

    inline void setTranslationModel(QString path) {
        translation_model_ = path;
        emit translationModelChanged(path);
    }

    inline QString translationModel() const {
        return translation_model_;
    }

    inline void setCores(unsigned int cores) {
        cpu_cores_ = cores;
        emit coresChanged(cores);
    }

    inline unsigned int cores() const {
        return cpu_cores_;
    }

    inline void setWorkspace(unsigned int workspace) {
        workspace_ = workspace;
        emit workspaceChanged(workspace);
    }

    inline unsigned int workspace() const {
        return workspace_;
    }

signals:
    void translationModelChanged(QString path);
    void coresChanged(unsigned int cores);
    void workspaceChanged(unsigned int workspace);
};
