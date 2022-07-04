#ifndef COMMANDLINEIFACE_H
#define COMMANDLINEIFACE_H

#include <QObject>
#include <QPointer>
#include <QTextStream>
#include <QCommandLineParser>
#include <QEventLoop>
#include "inventory/ModelManager.h"
#include "settings/Settings.h"
#include "MarianInterface.h"
#include "Translation.h"
#include "Network.h"

class CommandLineIface : public QObject {
    Q_OBJECT
private:
    // Event loop that would wait until translation completes
    QEventLoop eventLoop_;

    // Settings, network and translator:
    Network network_;
    Settings settings_;
    ModelManager models_;
    QPointer<MarianInterface> translator_;

    // do_once file in and file out
    QFile infile_;
    QFile outfile_;
    QTextStream instream_;
    QTextStream outstream_;

    static const int constexpr prefetchLines = 320;

    // Functions
    void printLocalModels();
    void doTranslation();
    void downloadRemoteModel(QString modelID);
    inline QString &fetchData(QString &);

    int allowNativeMessagingClient(QStringList ids);
    int removeNativeMessagingClient(QStringList ids);
    int listNativeMessagingClients();
    int updateNativeMessagingManifests();

public:
    explicit CommandLineIface(QObject * parent = nullptr);
    int run(QCommandLineParser const &);

private slots:
    void outputError(QString error);
    void outputTranslation(Translation output);
    void printRemoteModels();
};

#endif // COMMANDLINEIFACE_H
