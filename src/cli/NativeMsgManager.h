#pragma once
#include <QObject>
#include <QSet>
#include <QString>

class NativeMsgManager : public QObject {
public:
    bool writeNativeMessagingAppManifests(QSet<QString> nativeMessagingClients);
};
