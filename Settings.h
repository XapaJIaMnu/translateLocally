#pragma once
#include <QObject>
#include <QSettings>
#include "types.h"


class Settings : public QObject 
{
    Q_OBJECT
    Q_PROPERTY(unsigned int cores MEMBER cpu_cores_ NOTIFY coresChanged)
    Q_PROPERTY(unsigned int workspace MEMBER workspace_ NOTIFY workspaceChanged)

private:
    QSettings settings_;
    unsigned int cpu_cores_;
    unsigned int workspace_;
public:
    Settings(QObject *parent = nullptr);
    translateLocally::marianSettings marianSettings() const;

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
    void coresChanged(unsigned int cores);
    void workspaceChanged(unsigned int workspace);
};
