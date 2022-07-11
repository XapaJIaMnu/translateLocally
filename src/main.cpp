#include "mainwindow.h"
#include "version.h"
#include "3rd_party/bergamot-translator/3rd_party/marian-dev/src/marian.h"
#include "Translation.h"

#include <QApplication>
#include <QLoggingCategory>
#include <QTimer>
#include "cli/CLIParsing.h"
#include "cli/CommandLineIface.h"
#include "cli/NativeMsgIface.h"
#include "types.h"

int main(int argc, char *argv[])
{
    // Set marian to throw exceptions instead of std::abort()
    marian::setThrowExceptionOnAbort(true);

    // Register types so they can be used with Qt's signal/slots.
    qRegisterMetaType<Translation>("Translation");
    qRegisterMetaType<QVector<WordAlignment>>("QVector<WordAlignment>");
    qRegisterMetaType<Translation::Direction>("Translation::Direction");
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0)) // https://www.qt.io/blog/whats-new-in-qmetatype-qvariant
    qRegisterMetaTypeStreamOperators<QList<QStringList>>("QList<QStringList>");
    qRegisterMetaTypeStreamOperators<translateLocally::Repository>("translateLocally::Repository");
#endif

    // Check for CLIOnly mode. In CLIOnly mode we create QCoreApplication that doesn't require a display plugin.
    // In case we do not need CLIOnly mode, skip the command line parsing and go straght to the GUI instantiation.
    {
        QCoreApplication translateLocally(argc, argv);
        QCoreApplication::setApplicationName("translateLocally");
        QCoreApplication::setApplicationVersion(TRANSLATELOCALLY_VERSION_FULL);
        QCommandLineParser parser;
        translateLocally::CLIArgumentInit(translateLocally, parser);

        // Hide debug messages by default, unless `--debug` is passed in or if the user has defined the
        // QT_LOGGING_CONF or the QT_LOGGING_RULES environment variable.
        if (!parser.isSet("debug"))
             QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));

        // Launch application unless we're supposed to be in CLI mode
        translateLocally::AppType runtime = translateLocally::runType(parser);
        switch (runtime) {
            case translateLocally::AppType::CLI:
                return CommandLineIface().run(parser);
            case translateLocally::AppType::NativeMsg:
        {
                NativeMsgIface * nativeMSG = new NativeMsgIface(&translateLocally);
                QObject::connect(nativeMSG, &NativeMsgIface::finished, &translateLocally, &QCoreApplication::quit);
                QTimer::singleShot(0, nativeMSG, &NativeMsgIface::run);
                return translateLocally.exec();
        }
            case translateLocally::AppType::GUI:
                break; //Handled later outside this scope.
        }
    }

    QApplication translateLocally(argc, argv);
    QCoreApplication::setApplicationName("translateLocally");
    QCoreApplication::setApplicationVersion(TRANSLATELOCALLY_VERSION_FULL);

    MainWindow w;
    w.show();
    return translateLocally.exec();
}
