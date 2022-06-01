#pragma once
#include <QObject>
#include <QSet>
#include <QString>

class NativeMsgManager : public QObject {
public:
    bool registerNativeMessagingAppManifests(QSet<QString> nativeMessagingClients);
};
