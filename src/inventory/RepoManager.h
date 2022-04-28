#pragma once
#include <QObject>
#include "settings/Settings.h"

constexpr const char* kDefaultRepositoryName = "Bergamot";

constexpr const char* kDefaultRepositoryURL = "https://translatelocally.com/models.json";

struct Repository {
    QString name;
    QString url;
    bool isDefault;
};

class RepoManager : public QObject {
    Q_OBJECT
public:
    RepoManager(QObject * parent);
    /**
     * @brief getRepos gets all currently available repos, including default ones
     * @return List of repository objects
     */
    QList<Repository> getRepos() const;
    QString getName(QString url) const;

    /**
     * @Brief Fills list using data from Setting.
     */
    void load(QList<QStringList> stored);

private:
    QMap<QString,Repository> repositories_;
};
