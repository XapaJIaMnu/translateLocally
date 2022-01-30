#include "mainwindow.h"
#include "version.h"
#include "3rd_party/bergamot-translator/3rd_party/marian-dev/src/marian.h"
#include "Translation.h"

#include <QApplication>
#include "CLIParsing.h"
#include "CommandLineIface.h"

int main(int argc, char *argv[])
{
    // Set marian to throw exceptions instead of std::abort()
    marian::setThrowExceptionOnAbort(true);

    // Register types so they can be used with Qt's signal/slots.
    qRegisterMetaType<Translation>("Translation");
    qRegisterMetaType<QVector<WordAlignment>>("QVector<WordAlignment>");
    qRegisterMetaType<Translation::Direction>("Translation::Direction");
    qRegisterMetaType<QList<QStringList>>("QList<QStringList>");
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0)) // https://www.qt.io/blog/whats-new-in-qmetatype-qvariant
    qRegisterMetaTypeStreamOperators<QList<QStringList>>("QList<QStringList>");
#endif

    QApplication translateLocally(argc, argv);
    QCoreApplication::setApplicationName("translateLocally");
    QCoreApplication::setApplicationVersion(TRANSLATELOCALLY_VERSION_FULL);

    // Command line parsing
    QCommandLineParser parser;
    translateLocally::CLIArgumentInit(translateLocally, parser);

    // Launch application unless we're supposed to be in CLI mode
    if (translateLocally::isCLIOnly(parser)) {
        return CommandLineIface().run(parser); // Also takes care of exit codes
    } else {
        MainWindow w;
        w.show();
        return translateLocally.exec();
    }
}
